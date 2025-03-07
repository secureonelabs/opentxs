// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/json.hpp>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <random>
#include <string_view>

#include "internal/blockchain/node/wallet/FeeSource.hpp"
#include "internal/blockchain/node/wallet/Types.hpp"
#include "internal/util/Timer.hpp"
#include "opentxs/network/zeromq/Types.hpp"
#include "opentxs/util/Container.hpp"
#include "util/Actor.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs
{
namespace api
{
namespace internal
{
class Session;
}  // namespace internal

class Session;
}  // namespace api

namespace blockchain
{
namespace node
{
class Manager;
}  // namespace node
}  // namespace blockchain

namespace display
{
class Scale;
}  // namespace display

namespace network
{
namespace zeromq
{
namespace socket
{
class Raw;
}  // namespace socket
}  // namespace zeromq
}  // namespace network

class Amount;
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

class opentxs::blockchain::node::wallet::FeeSource::Imp
    : public Actor<Imp, FeeSourceJobs>
{
public:
    auto Init(std::shared_ptr<Imp> me) noexcept -> void { signal_startup(me); }

    ~Imp() override;

protected:
    const CString asio_;

    auto process_double(double rate, unsigned long long int scale) noexcept
        -> std::optional<Amount>;
    auto process_int(std::int64_t rate, unsigned long long int scale) noexcept
        -> std::optional<Amount>;
    auto shutdown_timers() noexcept -> void;

    Imp(std::shared_ptr<const api::internal::Session> api,
        std::shared_ptr<const node::Manager> node,
        std::string_view hostname,
        std::string_view path,
        bool https,
        network::zeromq::BatchID batch,
        allocator_type&& alloc) noexcept;

private:
    friend Actor<Imp, FeeSourceJobs>;

    std::shared_ptr<const api::internal::Session> api_p_;
    std::shared_ptr<const node::Manager> node_p_;
    const api::Session& api_;
    const node::Manager& node_;
    const CString hostname_;
    const CString path_;
    const bool https_;
    std::random_device rd_;
    std::default_random_engine eng_;
    std::uniform_int_distribution<int> dist_;
    network::zeromq::socket::Raw& to_oracle_;
    std::optional<std::future<boost::json::value>> future_;
    Timer timer_;

    static auto display_scale() -> const display::Scale&;

    auto jitter() noexcept -> std::chrono::seconds;
    virtual auto process(const boost::json::value& data) noexcept
        -> std::optional<Amount> = 0;

    auto do_shutdown() noexcept -> void;
    auto do_startup(allocator_type monotonic) noexcept -> bool;
    auto pipeline(const Work work, Message&& msg, allocator_type) noexcept
        -> void;
    auto query() noexcept -> void;
    auto reset_timer() noexcept -> void;
    auto work(allocator_type monotonic) noexcept -> bool;

    Imp(std::shared_ptr<const api::internal::Session> api,
        std::shared_ptr<const node::Manager> node,
        std::string_view hostname,
        std::string_view path,
        bool https,
        CString&& asio,
        network::zeromq::BatchID batch,
        allocator_type&& alloc) noexcept;
};
