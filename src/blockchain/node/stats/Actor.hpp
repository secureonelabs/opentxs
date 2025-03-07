// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>

#include "internal/util/PMR.hpp"
#include "opentxs/blockchain/node/Types.internal.hpp"
#include "opentxs/network/zeromq/Types.hpp"
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
namespace stats
{
class Shared;
}  // namespace stats
}  // namespace node
}  // namespace blockchain

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
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::node::stats
{
class Actor final : public opentxs::Actor<stats::Actor, StatsJobs>
{
public:
    auto Init(std::shared_ptr<Actor> self) noexcept -> void
    {
        signal_startup(self);
    }

    auto get_deleter() noexcept -> delete_function final
    {
        return pmr::make_deleter(this);
    }

    Actor(
        std::shared_ptr<const api::internal::Session> api,
        std::shared_ptr<Shared> shared,
        network::zeromq::BatchID batchID,
        allocator_type alloc) noexcept;
    Actor() = delete;
    Actor(const Actor&) = delete;
    Actor(Actor&&) = delete;
    auto operator=(const Actor&) -> Actor& = delete;
    auto operator=(Actor&&) -> Actor& = delete;

    ~Actor() final;

private:
    friend opentxs::Actor<stats::Actor, Work>;

    std::shared_ptr<const api::internal::Session> api_p_;
    std::shared_ptr<Shared> shared_;
    const api::Session& api_;
    Shared& data_;
    network::zeromq::socket::Raw& to_blockchain_api_;

    auto do_shutdown() noexcept -> void;
    auto do_startup(allocator_type monotonic) noexcept -> bool;
    auto pipeline(const Work work, Message&& msg, allocator_type) noexcept
        -> void;
    auto process_block(Message&& msg) noexcept -> void;
    auto process_block_header(Message&& msg) noexcept -> void;
    auto process_cfilter(Message&& msg) noexcept -> void;
    auto process_peer(Message&& msg) noexcept -> void;
    auto process_reorg(Message&& msg) noexcept -> void;
    auto process_sync_server(Message&& msg) noexcept -> void;
    auto work(allocator_type monotonic) noexcept -> bool;
};
}  // namespace opentxs::blockchain::node::stats
