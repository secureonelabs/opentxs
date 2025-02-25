// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstddef>
#include <future>
#include <memory>
#include <string_view>

#include "BoostAsio.hpp"
#include "api/network/asio/Acceptors.hpp"
#include "internal/api/network/Asio.hpp"
#include "opentxs/Types.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace boost
{
namespace json
{
class value;
}  // namespace json
}  // namespace boost

namespace opentxs
{
namespace api
{
namespace internal
{
class Context;
}  // namespace internal

namespace network
{
namespace asio
{
class Shared;
}  // namespace asio
}  // namespace network
}  // namespace api

namespace network
{
namespace zeromq
{
class Context;
class Envelope;
}  // namespace zeromq
}  // namespace network

class ByteArray;
class Timer;
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::api::network::implementation
{
class Asio final : public internal::Asio
{
public:
    auto Close(const Endpoint& endpoint) const noexcept -> bool final;
    auto FetchJson(
        const ReadView host,
        const ReadView path,
        const bool https,
        const ReadView notify) const noexcept
        -> std::future<boost::json::value> final;
    auto GetPublicAddress4() const noexcept
        -> std::shared_future<ByteArray> final;
    auto GetPublicAddress6() const noexcept
        -> std::shared_future<ByteArray> final;
    auto MakeSocket(const Endpoint& endpoint) const noexcept -> Socket final;
    auto NotificationEndpoint() const noexcept -> std::string_view final;
    auto Accept(const Endpoint& endpoint, AcceptCallback cb) const noexcept
        -> bool final;
    auto Connect(const opentxs::network::zeromq::Envelope& id, SocketImp socket)
        const noexcept -> bool final;
    auto GetTimer() const noexcept -> Timer final;
    auto IOContext() const noexcept -> boost::asio::io_context& final;
    auto Receive(
        const opentxs::network::zeromq::Envelope& id,
        const OTZMQWorkType type,
        const std::size_t bytes,
        SocketImp socket) const noexcept -> bool final;
    auto Transmit(
        const opentxs::network::zeromq::Envelope& id,
        const ReadView bytes,
        SocketImp socket) const noexcept -> bool final;

    auto Init(std::shared_ptr<const api::internal::Context> context) noexcept
        -> void final;
    auto Shutdown() noexcept -> void final;

    Asio(const opentxs::network::zeromq::Context& zmq, bool test) noexcept;
    Asio() = delete;
    Asio(const Asio&) = delete;
    Asio(Asio&&) = delete;
    auto operator=(const Asio&) -> Asio& = delete;
    auto operator=(Asio&&) -> Asio& = delete;

    ~Asio() final;

private:
    const bool test_;
    std::shared_ptr<asio::Shared> main_;
    std::weak_ptr<asio::Shared> weak_;
    mutable asio::Acceptors acceptors_;

    Asio(std::shared_ptr<asio::Shared> shared, const bool test) noexcept;
};
}  // namespace opentxs::api::network::implementation
