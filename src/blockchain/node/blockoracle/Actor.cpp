// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "blockchain/node/blockoracle/Actor.hpp"  // IWYU pragma: associated

#include <chrono>
#include <exception>
#include <memory>
#include <utility>
#include <variant>

#include "blockchain/node/blockoracle/Shared.hpp"
#include "internal/api/session/Endpoints.hpp"
#include "internal/blockchain/node/Endpoints.hpp"
#include "internal/blockchain/node/Manager.hpp"
#include "internal/network/zeromq/Pipeline.hpp"
#include "internal/network/zeromq/socket/Pipeline.hpp"
#include "internal/network/zeromq/socket/Raw.hpp"
#include "internal/util/P0330.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/WorkType.internal.hpp"
#include "opentxs/api/Session.hpp"
#include "opentxs/api/Session.internal.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/blockchain/node/Manager.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/message/Message.tpp"
#include "opentxs/network/zeromq/socket/Direction.hpp"   // IWYU pragma: keep
#include "opentxs/network/zeromq/socket/Policy.hpp"      // IWYU pragma: keep
#include "opentxs/network/zeromq/socket/SocketType.hpp"  // IWYU pragma: keep
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Writer.hpp"
#include "util/ScopeGuard.hpp"

namespace opentxs::blockchain::node::internal
{
using namespace std::literals;
using enum opentxs::network::zeromq::socket::Direction;
using opentxs::network::zeromq::socket::Policy;
using enum opentxs::network::zeromq::socket::Type;

BlockOracle::Actor::Actor(
    std::shared_ptr<const api::internal::Session> api,
    std::shared_ptr<const node::Manager> node,
    std::shared_ptr<Shared> shared,
    network::zeromq::BatchID batch,
    allocator_type alloc) noexcept
    : BlockOracleActor(
          api->Self(),
          shared->log_,
          CString{shared->name_, alloc},
          0ms,
          std::move(batch),
          alloc,
          {
              {api->Endpoints().Shutdown(), Connect},
              {api->Endpoints().BlockchainReorg(), Connect},
              {api->Endpoints().Internal().BlockchainReportStatus(), Connect},
              {node->Internal().Endpoints().shutdown_publish_, Connect},
          },
          {
              {node->Internal().Endpoints().block_oracle_pull_, Bind},
          },
          {},
          {
              {Router,
               Policy::Internal,
               {
                   {node->Internal().Endpoints().block_oracle_router_, Bind},
               }},
              {Publish,
               Policy::Internal,
               {
                   {node->Internal().Endpoints().block_tip_publish_, Bind},
               }},
              {Push,
               Policy::Internal,
               {
                   {api->Endpoints().Internal().BlockchainMessageRouter(),
                    Connect},
               }},
          })
    , api_p_(std::move(api))
    , node_p_(std::move(node))
    , shared_p_(std::move(shared))
    , api_(api_p_->Self())
    , node_(*node_p_)
    , shared_(*shared_p_)
    , router_(pipeline_.Internal().ExtraSocket(0))
    , tip_updated_(pipeline_.Internal().ExtraSocket(1))
    , to_blockchain_api_(pipeline_.Internal().ExtraSocket(2))
    , chain_(node_.Internal().Chain())
    , requests_(alloc)
    , downloader_(
          log_,
          name_,
          [this](const auto& tip) { set_tip(tip); },
          [](const auto&) {},
          alloc)
{
}

auto BlockOracle::Actor::broadcast_tip() noexcept -> void
{
    const auto& tip = downloader_.Tip();
    tip_updated_.SendDeferred([&] {
        auto msg = MakeWork(OT_ZMQ_NEW_FULL_BLOCK_SIGNAL);
        msg.AddFrame(tip.height_);
        msg.AddFrame(tip.hash_);

        return msg;
    }());
    to_blockchain_api_.SendDeferred([&] {
        auto msg = MakeWork(WorkType::BlockchainBlockOracleProgress);
        msg.AddFrame(chain_);
        msg.AddFrame(tip.height_);
        msg.AddFrame(tip.hash_);

        return msg;
    }());
}

auto BlockOracle::Actor::do_shutdown() noexcept -> void
{
    shared_p_.reset();
    node_p_.reset();
    api_p_.reset();
}

auto BlockOracle::Actor::do_startup(allocator_type monotonic) noexcept -> bool
{
    if ((api_.Internal().ShuttingDown()) || (node_.Internal().ShuttingDown())) {

        return true;
    }

    if (shared_.download_blocks_) {
        downloader_.SetTip(shared_.GetTip(monotonic));
        do_work(monotonic);
    }

    return false;
}

auto BlockOracle::Actor::Init(std::shared_ptr<Actor> me) noexcept -> void
{
    signal_startup(me);
}

auto BlockOracle::Actor::notify_requestors(
    std::span<const block::Hash> ids,
    std::span<const BlockLocation> blocks,
    allocator_type monotonic) noexcept -> void
{
    assert_true(ids.size() == blocks.size());

    auto out = Notifications{monotonic};

    for (auto n = 0_uz; n < ids.size(); ++n) {
        const auto& id = ids[n];
        const auto& block = blocks[n];
        notify_requestors(id, block, out);
    }

    notify_requestors(out);
}

auto BlockOracle::Actor::notify_requestors(
    const block::Hash& hash,
    const BlockLocation& data,
    Notifications& out) noexcept -> void
{
    if (false == is_valid(data)) { return; }

    if (auto req = requests_.find(hash); requests_.end() != req) {
        auto post = ScopeGuard{[&] { requests_.erase(req); }};

        for (const auto& connection : req->second) {
            auto& message = [&]() -> auto& {
                if (auto m = out.find(connection); out.end() != m) {

                    return m->second;
                } else {
                    auto [i, added] = out.try_emplace(
                        connection,
                        network::zeromq::tagged_reply_to_message(
                            connection, OT_ZMQ_BLOCK_ORACLE_BLOCK_READY, true));

                    assert_true(added);

                    return i->second;
                }
            }();
            message.AddFrame(hash);

            if (false == serialize(data, message.AppendBytes())) {
                LogAbort()().Abort();
            }
        }
    }
}

auto BlockOracle::Actor::notify_requestors(Notifications& messages) noexcept
    -> void
{
    for (auto& [_, message] : messages) {
        router_.SendDeferred(std::move(message));
    }
}

auto BlockOracle::Actor::pipeline(
    const Work work,
    Message&& msg,
    allocator_type monotonic) noexcept -> void
{
    using network::zeromq::SocketID;
    const auto socket = connection_id(msg);

    switch (work) {
        case Work::header: {
            process_header(std::move(msg));
        } break;
        case Work::reorg: {
            process_reorg(std::move(msg));
        } break;
        case Work::request_blocks: {
            if (router_.ID() == socket) {
                process_request_blocks(std::move(msg), monotonic);
            } else {
                LogAbort()()(name_)(" received ")(print(work))()(
                    " on pull socket")
                    .Abort();
            }
        } break;
        case Work::block_ready: {
            process_block_ready(std::move(msg), monotonic);
        } break;
        case Work::report: {
            process_report(std::move(msg));
        } break;
        case Work::submit_block: {
            process_submit_block(std::move(msg), monotonic);
        } break;
        case Work::shutdown:
        case Work::init:
        case Work::statemachine: {
            unhandled_type(work);
        }
        default: {
            unknown_type(work);
        }
    }

    if (shared_.download_blocks_) { do_work(monotonic); }
}

auto BlockOracle::Actor::process_block_ready(
    Message&& msg,
    allocator_type monotonic) noexcept -> void
{
    const auto body = msg.Payload();
    const auto count = body.size();

    if ((3_uz > count) || (0_uz == count % 2_uz)) {
        LogAbort()()(name_)(": invalid message frame count: ")(count).Abort();
    }

    auto done = Notifications{monotonic};
    done.clear();
    const auto cb = [&, this](const auto& hash, const auto& block) {
        notify_requestors(hash, block, done);
    };

    for (auto n = 1_uz; n < count; n += 2_uz) {
        const auto hash = block::Hash{body[n].Bytes()};
        const auto block = parse_block_location(body[n + 1_uz]);
        downloader_.ReceiveBlock(hash, block, cb);
    }

    notify_requestors(done);
    shared_.FinishWork();
}

auto BlockOracle::Actor::process_header(Message&& msg) noexcept -> void
{
    // NOTE no action required
}

auto BlockOracle::Actor::process_reorg(Message&& msg) noexcept -> void
{
    // NOTE no action required
}

auto BlockOracle::Actor::process_report(Message&& msg) noexcept -> void
{
    broadcast_tip();
}

auto BlockOracle::Actor::process_request_blocks(
    Message&& msg,
    allocator_type monotonic) noexcept -> void
{
    const auto requestor = msg.Envelope();
    const auto body = msg.Payload();
    const auto count = body.size();

    if (1_uz >= count) { return; }

    const auto hashes = [&] {
        auto out = Vector<block::Hash>{monotonic};
        out.reserve(count - 1_uz);

        for (auto n = 1_uz; n < count; ++n) {
            const auto& hash = out.emplace_back(body[n].Bytes());
            requests_[hash].emplace(requestor);
        }

        return out;
    }();
    const auto blocks = shared_.GetBlocks(hashes, monotonic, monotonic);
    notify_requestors(hashes, blocks, monotonic);
}

auto BlockOracle::Actor::process_submit_block(
    Message&& msg,
    allocator_type monotonic) noexcept -> void
{
    const auto body = msg.Payload();

    assert_true(1_uz < body.size());

    shared_.Receive(body[1].Bytes(), monotonic);
}

auto BlockOracle::Actor::queue_blocks(allocator_type monotonic) noexcept -> bool
{
    try {
        auto [height, hashes, more] =
            downloader_.AddBlocks(node_.HeaderOracle(), monotonic);
        const auto count = hashes.size();
        const auto blocks = shared_.GetBlocks(hashes, monotonic, monotonic);

        assert_true(blocks.size() == count);

        auto done = Notifications{monotonic};
        done.clear();
        const auto cb = [&, this](const auto& hash, const auto& block) {
            notify_requestors(hash, block, done);
        };

        for (auto n = 0_uz; n < count; ++n, ++height) {
            const auto& id = hashes[n];
            const auto& block = blocks[n];

            if (is_valid(block)) {
                downloader_.ReceiveBlock(id, block, cb, height);
            }
        }

        notify_requestors(done);
        downloader_.Update();

        return more;
    } catch (const std::exception& e) {
        LogError()()(name_)(": ")(e.what()).Flush();

        return true;
    }
}

auto BlockOracle::Actor::set_tip(const block::Position& tip) noexcept -> void
{
    if (shared_.SetTip(tip)) {
        downloader_.SetTip(tip);
        broadcast_tip();
    } else {
        LogAbort()()(name_)(": failed to update database").Abort();
    }
}

auto BlockOracle::Actor::work(allocator_type monotonic) noexcept -> bool
{
    if (shared_.download_blocks_) {

        return queue_blocks(monotonic);
    } else {

        return false;
    }
}

BlockOracle::Actor::~Actor() = default;
}  // namespace opentxs::blockchain::node::internal
