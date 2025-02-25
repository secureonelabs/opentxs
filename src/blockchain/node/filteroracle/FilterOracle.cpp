// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_forward_declare opentxs::blockchain::node::FilterOracle

#include "blockchain/node/filteroracle/FilterOracle.hpp"  // IWYU pragma: associated

#include <span>

#include "blockchain/node/filteroracle/Shared.hpp"
#include "internal/blockchain/node/Factory.hpp"
#include "opentxs/Context.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/cfilter/GCS.hpp"
#include "opentxs/blockchain/cfilter/Hash.hpp"  // IWYU pragma: keep
#include "opentxs/blockchain/cfilter/Header.hpp"
#include "opentxs/blockchain/node/FilterOracle.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"

namespace opentxs::factory
{
auto BlockchainFilterOracle(
    const api::Session& api,
    const blockchain::node::HeaderOracle& header,
    const blockchain::node::Endpoints& endpoints,
    const blockchain::node::internal::Config& config,
    blockchain::database::Cfilter& db,
    blockchain::Type chain,
    blockchain::cfilter::Type filter) noexcept
    -> std::unique_ptr<blockchain::node::FilterOracle>
{
    using ReturnType = opentxs::blockchain::node::implementation::FilterOracle;

    return std::make_unique<ReturnType>(
        api, header, endpoints, config, db, chain, filter);
}
}  // namespace opentxs::factory

namespace opentxs::blockchain::node::implementation
{
FilterOracle::FilterOracle(
    const api::Session& api,
    const node::HeaderOracle& header,
    const node::Endpoints& endpoints,
    const node::internal::Config& config,
    database::Cfilter& db,
    blockchain::Type chain,
    blockchain::cfilter::Type filter) noexcept
    : internal::FilterOracle()
    , shared_p_(std::make_shared<filteroracle::Shared>(
          api,
          header,
          endpoints,
          config,
          db,
          chain,
          filter))
    , shared_(*shared_p_)
{
    assert_false(nullptr == shared_p_);

    RunJob([shared = shared_p_] { shared->Init(); });
}

auto FilterOracle::FilterTip(const cfilter::Type type) const noexcept
    -> block::Position
{
    return shared_.CfilterTip(type);
}

auto FilterOracle::DefaultType() const noexcept -> cfilter::Type
{
    return shared_.default_type_;
}

auto FilterOracle::Heartbeat() noexcept -> void { shared_.Heartbeat(); }

auto FilterOracle::Init(
    std::shared_ptr<const api::internal::Session> api,
    std::shared_ptr<const node::Manager> node) noexcept -> void
{
    shared_.Init(api, node, shared_p_);
}

auto FilterOracle::LoadFilter(
    const cfilter::Type type,
    const block::Hash& block,
    alloc::Strategy alloc) const noexcept -> cfilter::GCS
{
    return shared_.LoadCfilter(type, block.Bytes(), alloc);
}

auto FilterOracle::LoadFilters(
    const cfilter::Type type,
    const Vector<block::Hash>& blocks,
    alloc::Strategy alloc) const noexcept -> Vector<cfilter::GCS>
{
    return shared_.LoadCfilters(type, blocks, alloc);
}

auto FilterOracle::LoadFilterHeader(
    const cfilter::Type type,
    const block::Hash& block) const noexcept -> cfilter::Header
{
    return shared_.LoadCfheader(type, block);
}

auto FilterOracle::ProcessBlock(
    const block::Block& block,
    alloc::Default monotonic) const noexcept -> bool
{
    return shared_.ProcessBlock(block, monotonic);
}

auto FilterOracle::ProcessSyncData(
    const block::Hash& prior,
    const Vector<block::Hash>& hashes,
    const network::otdht::Data& data,
    alloc::Default monotonic) const noexcept -> void
{
    shared_.ProcessSyncData(prior, hashes, data, monotonic);
}

auto FilterOracle::Tip(const cfilter::Type type) const noexcept
    -> block::Position
{
    return shared_.CfilterTip(type);
}

FilterOracle::~FilterOracle() = default;
}  // namespace opentxs::blockchain::node::implementation
