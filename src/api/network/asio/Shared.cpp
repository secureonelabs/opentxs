// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "api/network/asio/Shared.hpp"  // IWYU pragma: associated

#include <boost/algorithm/string.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/json.hpp>
#include <boost/system/error_code.hpp>
#include <cs_plain_guarded.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "BoostAsio.hpp"
#include "api/network/asio/Context.hpp"
#include "api/network/asio/Data.hpp"
#include "internal/api/network/Asio.hpp"
#include "internal/network/asio/HTTP.hpp"
#include "internal/network/asio/HTTPS.hpp"
#include "internal/network/asio/Types.hpp"
#include "internal/network/zeromq/Context.hpp"
#include "internal/network/zeromq/socket/Factory.hpp"
#include "internal/network/zeromq/socket/Raw.hpp"
#include "internal/util/Thread.hpp"
#include "internal/util/Timer.hpp"
#include "network/asio/Endpoint.hpp"
#include "network/asio/Socket.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/WorkType.internal.hpp"
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/network/asio/Endpoint.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/Types.hpp"
#include "opentxs/network/zeromq/message/Envelope.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/message/Message.tpp"
#include "opentxs/network/zeromq/socket/SocketType.hpp"  // IWYU pragma: keep
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"

namespace algo = boost::algorithm;

namespace opentxs::api::network::asio
{
using namespace std::literals;

Shared::Shared(const opentxs::network::zeromq::Context& zmq, bool test) noexcept
    : zmq_(zmq)
    , batch_id_(zmq_.Internal().PreallocateBatch())
    , endpoint_(opentxs::network::zeromq::MakeArbitraryInproc())
    , running_(false)
    , data_(zmq_, endpoint_, test)
{
}

auto Shared::Connect(
    std::shared_ptr<const Shared> me,
    const opentxs::network::zeromq::Envelope& id,
    internal::Asio::SocketImp socket) noexcept -> bool
{
    try {
        if (false == me.operator bool()) {
            throw std::runtime_error{"invalid self"};
        }

        if (false == socket.operator bool()) {
            throw std::runtime_error{"invalid socket"};
        }

        if (false == id.IsValid()) { throw std::runtime_error{"invalid id"}; }

        const auto handle = me->data_.lock_shared();
        [[maybe_unused]] const auto& data = *handle;

        if (false == me->running_) {
            throw std::runtime_error{"shutting down"};
        }

        const auto& endpoint = socket->endpoint_;
        const auto& internal = endpoint.GetInternal().data_;
        socket->socket_.async_connect(
            internal,
            [me, socket, connection{id}, address{endpoint.str()}](
                const auto& e) mutable {
                me->process_connect(socket, e, address, std::move(connection));
            });

        return true;
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return false;
    }
}

auto Shared::FetchJson(
    std::shared_ptr<const Shared> me,
    const ReadView host,
    const ReadView path,
    const bool https,
    const ReadView notify) noexcept -> std::future<boost::json::value>
{
    assert_false(nullptr == me);

    auto promise = std::make_shared<std::promise<boost::json::value>>();
    auto future = promise->get_future();
    auto f =
        (https) ? &Shared::retrieve_json_https : &Shared::retrieve_json_http;
    const auto handle = me->data_.lock_shared();
    const auto& data = *handle;
    using enum opentxs::network::asio::TLS;
    std::invoke(f, me, tls1_3, data, host, path, notify, std::move(promise));

    return future;
}

auto Shared::GetPublicAddress4() const noexcept -> std::shared_future<ByteArray>
{
    return data_.lock_shared()->ipv4_future_;
}

auto Shared::GetPublicAddress6() const noexcept -> std::shared_future<ByteArray>
{
    return data_.lock_shared()->ipv6_future_;
}

auto Shared::GetTimer() const noexcept -> Timer
{
    return opentxs::factory::Timer(data_.lock_shared()->io_context_);
}

auto Shared::Init() noexcept -> void
{
    auto handle = data_.lock();
    auto& data = *handle;
    const auto threads = MaxJobs();
    using enum ThreadPriority;
    data.io_context_->Init(std::max(threads / 8u, 1u), Normal);
    running_ = true;
}

auto Shared::IOContext() const noexcept -> boost::asio::io_context&
{
    return *(data_.lock()->io_context_);
}

auto Shared::post(const Data& data, internal::Asio::Callback cb) const noexcept
    -> bool
{
    assert_false(nullptr == cb);

    if (false == running_) { return false; }

    boost::asio::post(data.io_context_->get(), [action = std::move(cb)] {
        std::invoke(action);
    });

    return true;
}

auto Shared::process_address_query(
    const ResponseType type,
    std::shared_ptr<std::promise<ByteArray>> promise,
    std::future<Response> future) const noexcept -> void
{
    if (!promise) { return; }

    try {
        const auto string = [&] {
            auto output = CString{};
            const auto body = future.get().body();

            switch (type) {
                case ResponseType::IPvonly: {
                    auto parts = Vector<CString>{};
                    algo::split(parts, body, algo::is_any_of(","));

                    if (parts.size() > 1) { output = parts[1]; }
                } break;
                case ResponseType::AddressOnly: {
                    output = body;
                } break;
                default: {
                    throw std::runtime_error{"Unknown response type"};
                }
            }

            return output;
        }();

        if (string.empty()) { throw std::runtime_error{"Empty response"}; }

        using opentxs::network::asio::address_from_string;
        const auto address = address_from_string(string);

        if (false == address.has_value()) {
            const auto error =
                CString{"error parsing ip address: "}.append(string);

            throw std::runtime_error{error.c_str()};
        }

        LogVerbose()()("GET response: IP address: ")(string).Flush();

        if (address->is_v4()) {
            const auto bytes = address->to_v4().to_bytes();
            promise->set_value(ByteArray{bytes.data(), bytes.size()});
        } else if (address->is_v6()) {
            const auto bytes = address->to_v6().to_bytes();
            promise->set_value(ByteArray{bytes.data(), bytes.size()});
        }
    } catch (...) {
        promise->set_exception(std::current_exception());
    }
}

auto Shared::process_connect(
    internal::Asio::SocketImp,
    const boost::system::error_code& e,
    ReadView address,
    opentxs::network::zeromq::Envelope&& connection) const noexcept -> void
{
    data_.lock()->to_actor_.SendDeferred([&] {
        if (e) {
            LogVerbose()()("asio connect error: ")(e.message()).Flush();
            auto work = opentxs::network::zeromq::tagged_reply_to_message(
                std::move(connection), WorkType::AsioDisconnect, true);
            work.AddFrame(address.data(), address.size());
            work.AddFrame(e.message());

            return work;
        } else {
            auto work = opentxs::network::zeromq::tagged_reply_to_message(
                std::move(connection), WorkType::AsioConnect, true);
            work.AddFrame(address.data(), address.size());

            return work;
        }
    }());
}

auto Shared::process_json(
    const Data& data,
    const ReadView notify,
    std::shared_ptr<std::promise<boost::json::value>> promise,
    std::future<Response> future) const noexcept -> void
{
    if (!promise) { return; }

    try {
        const auto body = future.get().body();
        auto parser = boost::json::parser{};
        parser.write_some(body);
        promise->set_value(parser.release());
    } catch (...) {
        promise->set_exception(std::current_exception());
    }

    send_notification(data, notify);
}

auto Shared::process_receive(
    internal::Asio::SocketImp socket,
    const boost::system::error_code& e,
    ReadView address,
    opentxs::network::zeromq::Envelope&& connection,
    OTZMQWorkType type,
    std::size_t index,
    ReadView data) const noexcept -> void
{
    assert_false(nullptr == socket);

    data_.lock()->to_actor_.SendDeferred([&]() {
        auto work = opentxs::network::zeromq::tagged_reply_to_message(
            std::move(connection),
            (e ? value(WorkType::AsioDisconnect) : type),
            true);

        if (e) {
            work.AddFrame(address.data(), address.size());
            work.AddFrame(e.message());
        } else {
            work.AddFrame(data.data(), data.size());
        }

        assert_true(1 < work.Payload().size());

        return work;
    }());
    socket->buffer_.lock()->Finish(index);
}

auto Shared::process_resolve(
    const std::shared_ptr<Resolver>&,
    const boost::system::error_code& e,
    const Resolver::results_type& results,
    std::string_view server,
    std::uint16_t port,
    opentxs::network::zeromq::Envelope&& connection) const noexcept -> void
{
    data_.lock()->to_actor_.SendDeferred([&] {
        static constexpr auto trueValue = std::byte{0x01};
        static constexpr auto falseValue = std::byte{0x00};
        auto work = opentxs::network::zeromq::tagged_reply_to_message(
            std::move(connection), value(WorkType::AsioResolve), true);

        if (e) {
            work.AddFrame(falseValue);
            work.AddFrame(server.data(), server.size());
            work.AddFrame(port);
            work.AddFrame(e.message());
        } else {
            work.AddFrame(trueValue);
            work.AddFrame(server.data(), server.size());
            work.AddFrame(port);

            for (const auto& result : results) {
                const auto address = result.endpoint().address();

                if (address.is_v4()) {
                    const auto bytes = address.to_v4().to_bytes();
                    work.AddFrame(bytes.data(), bytes.size());
                } else {
                    const auto bytes = address.to_v6().to_bytes();
                    work.AddFrame(bytes.data(), bytes.size());
                }
            }
        }

        return work;
    }());
}

auto Shared::process_transmit(
    internal::Asio::SocketImp socket,
    const boost::system::error_code& e,
    std::size_t bytes,
    opentxs::network::zeromq::Envelope&& connection,
    std::size_t index) const noexcept -> void
{
    assert_false(nullptr == socket);

    data_.lock()->to_actor_.SendDeferred([&] {
        auto work = opentxs::network::zeromq::tagged_reply_to_message(
            std::move(connection), value(WorkType::AsioSendResult), true);
        work.AddFrame(bytes);
        static constexpr auto trueValue = std::byte{0x01};
        static constexpr auto falseValue = std::byte{0x00};

        if (e) {
            work.AddFrame(falseValue);
            work.AddFrame(e.message());
        } else {
            work.AddFrame(trueValue);
        }

        return work;
    }());
    socket->buffer_.lock()->Finish(index);
}

auto Shared::Receive(
    std::shared_ptr<const Shared> me,
    const opentxs::network::zeromq::Envelope& id,
    const OTZMQWorkType type,
    const std::size_t bytes,
    internal::Asio::SocketImp socket) noexcept -> bool
{
    try {
        if (false == me.operator bool()) {
            throw std::runtime_error{"invalid self"};
        }

        if (false == socket.operator bool()) {
            throw std::runtime_error{"invalid socket"};
        }

        if (false == id.IsValid()) { throw std::runtime_error{"invalid id"}; }

        if (false == me->running_) {
            throw std::runtime_error{"shutting down"};
        }

        const auto& endpoint = socket->endpoint_;
        auto* params =
            socket->buffer_.lock()->Receive(id, type, endpoint.str(), bytes);
        boost::asio::async_read(
            socket->socket_,
            std::get<1>(*params),
            [me, socket, params](const auto& e, auto size) {
                assert_false(nullptr == me);
                assert_false(nullptr == socket);
                assert_false(nullptr == params);

                auto& [index, buffer, address, work, replyTo] = *params;
                me->process_receive(
                    socket,
                    e,
                    address,
                    std::move(replyTo),
                    work,
                    index,
                    {static_cast<const char*>(buffer.data()), size});
            });

        return true;
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return false;
    }
}

auto Shared::Resolve(
    std::shared_ptr<const Shared> me,
    const opentxs::network::zeromq::Envelope& id,
    std::string_view server,
    std::uint16_t port) noexcept -> void
{
    try {
        if (false == me.operator bool()) {
            throw std::runtime_error{"invalid self"};
        }

        auto handle = me->data_.lock();
        auto& data = *handle;

        if (false == me->running_) { return; }

        auto& resolver = *data.resolver_;
        resolver.async_resolve(
            server,
            std::to_string(port),
            [me,
             port,
             connection{id},
             p = data.resolver_,
             query = UnallocatedCString{server}](
                const auto& e, const auto& results) mutable {
                me->process_resolve(
                    p, e, results, query, port, std::move(connection));
            });
    } catch (const std::exception& e) {
        LogVerbose()()(e.what()).Flush();
    }
}

auto Shared::retrieve_address_async(
    const Data& data,
    const Site& site,
    std::shared_ptr<std::promise<ByteArray>> pPromise) const noexcept -> void
{
    using HTTP = opentxs::network::asio::HTTP;
    post(
        data,
        [job = std::make_shared<HTTP>(
             site.host_,
             site.target_,
             *data.io_context_,
             [this, promise = std::move(pPromise), type = site.response_type_](
                 auto&& future) mutable {
                 process_address_query(
                     type, std::move(promise), std::move(future));
             })] { job->Start(); });
}

auto Shared::retrieve_address_async_ssl(
    opentxs::network::asio::TLS tls,
    const Data& data,
    const Site& site,
    std::shared_ptr<std::promise<ByteArray>> pPromise) const noexcept -> void
{
    using HTTPS = opentxs::network::asio::HTTPS;
    post(
        data,
        [job = std::make_shared<HTTPS>(
             tls,
             site.host_,
             site.target_,
             *data.io_context_,
             [this, promise = std::move(pPromise), type = site.response_type_](
                 auto&& future) mutable {
                 process_address_query(
                     type, std::move(promise), std::move(future));
             })] { job->Start(); });
}

auto Shared::retrieve_json_http(
    std::shared_ptr<const Shared> me,
    opentxs::network::asio::TLS,
    const Data& data,
    const ReadView host,
    const ReadView path,
    const ReadView notify,
    std::shared_ptr<std::promise<boost::json::value>> pPromise) noexcept -> void
{
    using HTTP = opentxs::network::asio::HTTP;
    me->post(
        data,
        [job = std::make_shared<HTTP>(
             host,
             path,
             *data.io_context_,
             [me,
              promise = std::move(pPromise),
              socket = UnallocatedCString{notify}](auto&& future) mutable {
                 auto handle = me->data_.try_lock_shared_for(10ms);

                 while (false == handle.operator bool()) {
                     if (false == me->running_) { return; }

                     handle = me->data_.try_lock_shared_for(10ms);
                 }

                 me->process_json(
                     *handle, socket, std::move(promise), std::move(future));
             })] { job->Start(); });
}

auto Shared::retrieve_json_https(
    std::shared_ptr<const Shared> me,
    opentxs::network::asio::TLS tls,
    const Data& data,
    const ReadView host,
    const ReadView path,
    const ReadView notify,
    std::shared_ptr<std::promise<boost::json::value>> pPromise) noexcept -> void
{
    using HTTPS = opentxs::network::asio::HTTPS;
    me->post(
        data,
        [job = std::make_shared<HTTPS>(
             tls,
             host,
             path,
             *data.io_context_,
             [me,
              promise = std::move(pPromise),
              socket = UnallocatedCString{notify}](auto&& future) mutable {
                 auto handle = me->data_.try_lock_shared_for(10ms);

                 while (false == handle.operator bool()) {
                     if (false == me->running_) { return; }

                     handle = me->data_.try_lock_shared_for(10ms);
                 }
                 me->process_json(
                     *handle, socket, std::move(promise), std::move(future));
             })] { job->Start(); });
}

auto Shared::send_notification(const Data& data, const ReadView notify)
    const noexcept -> void
{
    if (false == valid(notify)) { return; }

    try {
        const auto endpoint = CString{notify};
        auto& socket = [&]() -> auto& {
            auto handle = data.notify_.lock();
            auto& map = *handle;

            if (auto it = map.find(endpoint); map.end() != it) {

                return it->second;
            }

            auto [it, added] = map.try_emplace(endpoint, [&] {
                auto out = factory::ZMQSocket(
                    zmq_, opentxs::network::zeromq::socket::Type::Publish);
                const auto rc = out.Connect(endpoint.data());

                if (false == rc) {
                    throw std::runtime_error{
                        "Failed to connect to notification endpoint"};
                }

                return out;
            }());

            return it->second;
        }();
        LogTrace()()("notifying ")(endpoint).Flush();
        const auto rc =
            socket.lock()->Send(MakeWork(OT_ZMQ_STATE_MACHINE_SIGNAL));

        if (false == rc) {
            throw std::runtime_error{"Failed to send notification"};
        }
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return;
    }
}

auto Shared::StateMachine() noexcept -> bool
{
    auto again{false};

    {
        auto handle = data_.lock();
        auto& data = *handle;
        data.ipv4_promise_ = {};
        data.ipv6_promise_ = {};
        data.ipv4_future_ = data.ipv4_promise_.get_future();
        data.ipv6_future_ = data.ipv6_promise_.get_future();
    }

    auto futures4 = UnallocatedVector<std::future<ByteArray>>{};
    auto futures6 = UnallocatedVector<std::future<ByteArray>>{};

    {
        auto handle = data_.try_lock_shared_for(10ms);

        while (false == handle.operator bool()) {
            if (false == running_) { return false; }

            handle = data_.try_lock_shared_for(10ms);
        }

        const auto& data = *handle;

        for (const auto& site : sites()) {
            auto promise = std::make_shared<std::promise<ByteArray>>();

            if (IPversion::IPV4 == site.protocol_) {
                futures4.emplace_back(promise->get_future());

                if (site.tls_.has_value()) {
                    retrieve_address_async_ssl(
                        *site.tls_, data, site, std::move(promise));
                } else {
                    retrieve_address_async(data, site, std::move(promise));
                }
            } else {
                futures6.emplace_back(promise->get_future());

                if (site.tls_.has_value()) {
                    retrieve_address_async_ssl(
                        *site.tls_, data, site, std::move(promise));
                } else {
                    retrieve_address_async(data, site, std::move(promise));
                }
            }
        }
    }

    auto result4 = ByteArray{};
    auto result6 = ByteArray{};
    static constexpr auto limit = 15s;
    static constexpr auto ready = std::future_status::ready;

    for (auto& future : futures4) {
        try {
            if (const auto status = future.wait_for(limit); ready == status) {
                auto result = future.get();

                if (result.empty()) { continue; }

                result4 = std::move(result);
                break;
            }
        } catch (...) {
            try {
                auto eptr = std::current_exception();

                if (eptr) { std::rethrow_exception(eptr); }
            } catch (const std::exception& e) {
                LogVerbose()()(e.what()).Flush();
            }
        }
    }

    for (auto& future : futures6) {
        try {
            if (const auto status = future.wait_for(limit); ready == status) {
                auto result = future.get();

                if (result.empty()) { continue; }

                result6 = std::move(result);
                break;
            }
        } catch (...) {
            try {
                auto eptr = std::current_exception();

                if (eptr) { std::rethrow_exception(eptr); }
            } catch (const std::exception& e) {
                LogVerbose()()(e.what()).Flush();
            }
        }
    }

    if (result4.empty() && result6.empty()) { again = true; }

    {
        auto handle = data_.lock();
        auto& data = *handle;
        data.ipv4_promise_.set_value(std::move(result4));
        data.ipv6_promise_.set_value(std::move(result6));
    }

    LogTrace()()("Finished checking ip addresses").Flush();

    return again;
}

auto Shared::Transmit(
    std::shared_ptr<const Shared> me,
    const opentxs::network::zeromq::Envelope& id,
    const ReadView bytes,
    internal::Asio::SocketImp socket) noexcept -> bool
{
    try {
        if (false == me.operator bool()) {
            throw std::runtime_error{"invalid self"};
        }

        if (false == socket.operator bool()) {
            throw std::runtime_error{"invalid socket"};
        }

        if (false == id.IsValid()) { throw std::runtime_error{"invalid id"}; }

        const auto handle = me->data_.lock_shared();
        const auto& data = *handle;

        if (false == me->running_) { return false; }

        auto* params = socket->buffer_.lock()->Transmit(id, bytes);

        return me->post(data, [me, socket, params] {
            boost::asio::async_write(
                socket->socket_,
                std::get<1>(*params),
                [me, socket, params](
                    const boost::system::error_code& e, std::size_t count) {
                    assert_false(nullptr == me);
                    assert_false(nullptr == socket);
                    assert_false(nullptr == params);

                    auto& [index, buffer, replyTo] = *params;
                    me->process_transmit(
                        socket, e, count, std::move(replyTo), index);
                });
        });
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return false;
    }
}

Shared::~Shared()
{
    running_ = false;

    {
        auto handle = data_.lock();
        auto& data = *handle;
        data.resolver_.reset();
        data.io_context_->Stop();
    }
}
}  // namespace opentxs::api::network::asio
