// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_node_map.hpp>
#include <boost/unordered/unordered_node_set.hpp>
#include <cs_plain_guarded.h>
#include <functional>
#include <optional>

#include "blockchain/database/wallet/Pattern.hpp"     // IWYU pragma: keep
#include "blockchain/database/wallet/Position.hpp"    // IWYU pragma: keep
#include "blockchain/database/wallet/SubchainID.hpp"  // IWYU pragma: keep
#include "internal/blockchain/database/Types.hpp"
#include "internal/util/P0330.hpp"
#include "internal/util/storage/lmdb/Types.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/blockchain/block/Types.internal.hpp"
#include "opentxs/blockchain/cfilter/Types.hpp"
#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/crypto/Types.hpp"
#include "opentxs/identifier/Account.hpp"
#include "opentxs/identifier/Generic.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Numbers.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs
{
namespace api
{
class Session;
}  // namespace api

namespace blockchain
{
namespace block
{
class Position;
}  // namespace block
}  // namespace blockchain

namespace storage
{
namespace lmdb
{
class Database;
class Transaction;
}  // namespace lmdb
}  // namespace storage
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::database::wallet
{
using Mode = storage::lmdb::Mode;

constexpr auto id_index_{Table::SubchainIDTable};
constexpr auto last_indexed_{Table::SubchainLastIndexed};
constexpr auto last_scanned_{Table::SubchainLastScanned};
constexpr auto match_index_{Table::SubchainMatches};
constexpr auto pattern_index_{Table::SubchainPatterns};
constexpr auto patterns_{Table::WalletPatterns};
constexpr auto subchain_config_{Table::Config};

class SubchainCache
{
public:
    using dbPatterns =
        boost::unordered_node_set<db::Pattern, std::hash<db::Pattern>>;
    using dbPatternIndex = Set<ElementID>;

    auto DecodeIndex(const SubchainID& key) const noexcept(false)
        -> const db::SubchainID&;
    auto GetIndex(
        const SubaccountID& subaccount,
        const crypto::Subchain subchain,
        const cfilter::Type type,
        const VersionNumber version,
        storage::lmdb::Transaction& tx) const noexcept -> SubchainID;
    auto GetLastIndexed(const SubchainID& subchain) const noexcept
        -> std::optional<crypto::Bip32Index>;
    auto GetLastScanned(const SubchainID& subchain) const noexcept
        -> block::Position;
    auto GetPattern(const ElementID& id) const noexcept -> const dbPatterns&;
    auto GetPatternIndex(const SubchainID& id) const noexcept
        -> const dbPatternIndex&;

    auto AddPattern(
        const ElementID& id,
        const crypto::Bip32Index index,
        const ReadView data,
        storage::lmdb::Transaction& tx) noexcept -> bool;
    auto AddPatternIndex(
        const SubchainID& key,
        const ElementID& value,
        storage::lmdb::Transaction& tx) noexcept -> bool;
    auto Clear() noexcept -> void;
    auto SetLastIndexed(
        const SubchainID& subchain,
        const crypto::Bip32Index value,
        storage::lmdb::Transaction& tx) noexcept -> bool;
    auto SetLastScanned(
        const SubchainID& subchain,
        const block::Position& value,
        storage::lmdb::Transaction& tx) noexcept -> bool;

    SubchainCache(
        const api::Session& api,
        const storage::lmdb::Database& lmdb) noexcept;

    ~SubchainCache();

private:
    static constexpr auto reserve_ = 1000_uz;

    using SubchainIDMap = boost::
        unordered_node_map<SubchainID, db::SubchainID, std::hash<SubchainID>>;
    using LastIndexedMap = boost::unordered_flat_map<
        SubchainID,
        crypto::Bip32Index,
        std::hash<SubchainID>>;
    using LastScannedMap = boost::
        unordered_node_map<SubchainID, db::Position, std::hash<SubchainID>>;
    using PatternsMap =
        boost::unordered_node_map<ElementID, dbPatterns, std::hash<ElementID>>;
    using PatternIndexMap = boost::
        unordered_node_map<SubchainID, dbPatternIndex, std::hash<SubchainID>>;
    using GuardedSubchainID = libguarded::plain_guarded<SubchainIDMap>;
    using GuardedLastIndexed = libguarded::plain_guarded<LastIndexedMap>;
    using GuardedLastScanned = libguarded::plain_guarded<LastScannedMap>;
    using GuardedPatterns = libguarded::plain_guarded<PatternsMap>;
    using GuardedPatternIndex = libguarded::plain_guarded<PatternIndexMap>;

    const api::Session& api_;
    const storage::lmdb::Database& lmdb_;
    mutable GuardedSubchainID subchain_id_;
    mutable GuardedLastIndexed last_indexed_;
    mutable GuardedLastScanned last_scanned_;
    mutable GuardedPatterns patterns_;
    mutable GuardedPatternIndex pattern_index_;

    auto load_index(const SubchainID& key, SubchainIDMap& map) const
        noexcept(false) -> const db::SubchainID&;
    auto load_last_indexed(const SubchainID& key, LastIndexedMap& map) const
        noexcept(false) -> const crypto::Bip32Index&;
    auto load_last_scanned(const SubchainID& key, LastScannedMap& map) const
        noexcept(false) -> const db::Position&;
    auto load_pattern(const ElementID& key, PatternsMap& map) const noexcept
        -> const dbPatterns&;
    auto load_pattern_index(const SubchainID& key, PatternIndexMap& map)
        const noexcept -> const dbPatternIndex&;
    auto subchain_index(
        const SubaccountID& subaccount,
        const crypto::Subchain subchain,
        const cfilter::Type type,
        const VersionNumber version) const noexcept -> SubchainID;
};
}  // namespace opentxs::blockchain::database::wallet
