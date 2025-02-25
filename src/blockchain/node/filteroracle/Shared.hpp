// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/blockchain/cfilter/Hash.hpp"

#pragma once

#include <cs_shared_guarded.h>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <span>
#include <tuple>
#include <utility>

#include "blockchain/node/filteroracle/Data.hpp"
#include "internal/blockchain/database/Cfilter.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/cfilter/Types.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/Container.hpp"

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
namespace block
{
class Block;
class Hash;
class Position;
}  // namespace block

namespace cfilter
{
class GCS;
class Hash;
class Header;
}  // namespace cfilter

namespace node
{
namespace internal
{
struct Config;
}  // namespace internal

class HeaderOracle;
class Manager;
struct Endpoints;
}  // namespace node
}  // namespace blockchain

namespace network
{
namespace otdht
{
class Data;
}  // namespace otdht
}  // namespace network

class Log;
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::node::filteroracle
{
class Shared
{
public:
    using GuardedData = libguarded::shared_guarded<Data, std::shared_mutex>;
    using BestPosition =
        std::tuple<cfilter::Header, cfilter::Header, block::Position>;

    const api::Session& api_;
    const node::HeaderOracle& header_;
    const Log& log_;
    const blockchain::Type chain_;
    const cfilter::Type default_type_;
    const bool server_mode_;
    const bool standalone_mode_;

    auto CfheaderTip() const noexcept -> block::Position;
    auto CfheaderTip(const cfilter::Type type) const noexcept
        -> block::Position;
    auto CfilterTip() const noexcept -> block::Position;
    auto CfilterTip(const cfilter::Type type) const noexcept -> block::Position;
    auto FindBestPosition(const block::Position& candidate) const noexcept
        -> BestPosition;
    auto LoadCfheader(const cfilter::Type type, const block::Hash& block)
        const noexcept -> cfilter::Header;
    auto LoadCfilter(
        const cfilter::Type type,
        const block::Hash& block,
        alloc::Strategy alloc) const noexcept -> cfilter::GCS;
    auto LoadCfilterHash(const block::Hash& block, const Data& data)
        const noexcept -> cfilter::Hash;
    auto LoadCfilters(
        const cfilter::Type type,
        std::span<const block::Hash> blocks,
        alloc::Strategy alloc) const noexcept -> Vector<cfilter::GCS>;
    auto ProcessBlock(
        const cfilter::Type type,
        const block::Block& block,
        alloc::Default alloc,
        alloc::Default monotonic) const noexcept -> cfilter::GCS;
    auto Tips() const noexcept -> std::pair<block::Position, block::Position>;
    auto ValidateAgainstCheckpoint(
        const block::Position& block,
        const cfilter::Header& cfheader) noexcept -> block::Position;

    auto Heartbeat() noexcept -> void;
    auto Init() noexcept -> void;
    auto Init(
        std::shared_ptr<const api::internal::Session> api,
        std::shared_ptr<const node::Manager> node,
        std::shared_ptr<Shared> shared) noexcept -> void;
    auto Lock() noexcept -> GuardedData::handle;
    auto ProcessBlock(
        const block::Block& block,
        alloc::Default monotonic) noexcept -> bool;
    auto ProcessSyncData(
        const block::Hash& prior,
        std::span<const block::Hash> hashes,
        const network::otdht::Data& in,
        alloc::Default monotonic) noexcept -> void;
    auto Report() noexcept -> void;
    auto SetCfheaderTip(
        const cfilter::Type type,
        const block::Position& tip) noexcept -> bool;
    auto SetCfilterTip(
        const cfilter::Type type,
        const block::Position& tip) noexcept -> bool;
    auto SetTips(const block::Position& tip) noexcept -> bool;
    auto SetTips(const cfilter::Type type, const block::Position& tip) noexcept
        -> bool;
    auto StoreCfheaders(
        const cfilter::Type type,
        const cfilter::Header& previous,
        Vector<database::Cfilter::CFHeaderParams>&& headers) noexcept -> bool;
    auto StoreCfilters(
        const cfilter::Type type,
        const block::Position& tip,
        Vector<database::Cfilter::CFHeaderParams>&& headers,
        Vector<database::Cfilter::CFilterParams>&& filters,
        alloc::Strategy alloc) noexcept -> bool;
    auto UpdateCfilterTip(const block::Position& tip) noexcept -> void;
    auto UpdateCfilterTip(
        const cfilter::Type type,
        const block::Position& tip) noexcept -> void;

    Shared(
        const api::Session& api,
        const node::HeaderOracle& header,
        const node::Endpoints& endpoints,
        const node::internal::Config& config,
        database::Cfilter& db,
        blockchain::Type chain,
        blockchain::cfilter::Type type) noexcept;
    Shared() = delete;
    Shared(const Shared&) = delete;
    Shared(Shared&&) = delete;
    auto operator=(const Shared&) -> Shared& = delete;
    auto operator=(Shared&&) -> Shared& = delete;

    ~Shared();

private:
    GuardedData data_;

    static auto process_block(
        const api::Session& api,
        const cfilter::Type type,
        const block::Block& block,
        alloc::Default alloc,
        alloc::Default monotonic) noexcept -> cfilter::GCS;

    auto broadcast_cfilter_tip(
        const cfilter::Type type,
        const block::Position& tip,
        Data& data) const noexcept -> void;
    auto cfheader_tip(const cfilter::Type type, const Data& data) const noexcept
        -> block::Position;
    auto cfilter_tip(const cfilter::Type type, const Data& data) const noexcept
        -> block::Position;
    auto cfilter_tip_needs_broadcast(
        const cfilter::Type type,
        const block::Position& tip,
        Data& data) const noexcept -> bool;
    auto compare_cfheader_tip_to_checkpoint(Data& data, block::Position& tip)
        const noexcept -> void;
    auto find_acceptable_cfheader(
        const Data& data,
        block::Position& tip) noexcept -> void;
    auto find_acceptable_cfilter(
        const block::Position& cfheaderTip,
        const Data& data,
        block::Position& tip) noexcept -> void;
    auto find_best_position(block::Position candidate, const Data& data)
        const noexcept -> BestPosition;
    auto load_cfheader(
        const cfilter::Type type,
        const block::Hash& block,
        const Data& data) const noexcept -> cfilter::Header;
    auto load_cfilter(
        const cfilter::Type type,
        const block::Hash& block,
        const Data& data,
        alloc::Strategy alloc) const noexcept -> cfilter::GCS;
    auto load_cfilter_hash(
        const cfilter::Type type,
        const block::Hash& block,
        const Data& data) const noexcept -> cfilter::Hash;
    auto load_cfilters(
        const cfilter::Type type,
        std::span<const block::Hash> blocks,
        const Data& data,
        alloc::Strategy alloc) const noexcept -> Vector<cfilter::GCS>;
    auto process_sync_data(
        const block::Hash& prior,
        std::span<const block::Hash> hashes,
        const network::otdht::Data& in,
        Data& data,
        alloc::Default monotonic) const noexcept -> void;
    auto reset_tips_to(
        const cfilter::Type type,
        const block::Position& headerTip,
        const block::Position& filterTip,
        const block::Position& position,
        Data& data,
        std::optional<bool> resetHeader = std::nullopt,
        std::optional<bool> resetfilter = std::nullopt) const noexcept -> bool;
    auto reset_tips_to(
        const cfilter::Type type,
        const block::Position& headerTip,
        const block::Position& position,
        Data& data,
        const std::optional<bool> resetHeader = std::nullopt) const noexcept
        -> bool;
    auto reset_tips_to(
        const cfilter::Type type,
        const block::Position& position,
        Data& data,
        const std::optional<bool> resetHeader = std::nullopt,
        const std::optional<bool> resetfilter = std::nullopt) const noexcept
        -> bool;
    auto set_cfheader_tip(
        const cfilter::Type type,
        const block::Position& tip,
        Data& data) const noexcept -> bool;
    auto set_cfilter_tip(
        const cfilter::Type type,
        const block::Position& tip,
        Data& data) const noexcept -> bool;
    auto store_cfheaders(
        const cfilter::Type type,
        const cfilter::Header& previous,
        Vector<database::Cfilter::CFHeaderParams>&& headers,
        Data& data) const noexcept -> bool;
    auto store_cfilters(
        const cfilter::Type type,
        Vector<database::Cfilter::CFilterParams>&& filters,
        Data& data,
        alloc::Strategy alloc) const noexcept -> bool;
    auto store_cfilters(
        const cfilter::Type type,
        const block::Position& tip,
        Vector<database::Cfilter::CFHeaderParams>&& headers,
        Vector<database::Cfilter::CFilterParams>&& filters,
        Data& data,
        alloc::Strategy alloc) const noexcept -> bool;
    auto update_cfilter_tip(
        const cfilter::Type type,
        const block::Position& tip,
        Data& data) const noexcept -> void;
};
}  // namespace opentxs::blockchain::node::filteroracle
