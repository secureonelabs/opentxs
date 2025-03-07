// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include <boost/intrusive/detail/iterator.hpp>

#pragma once

#include <cs_plain_guarded.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

#include "blockchain/protocol/bitcoin/base/block/input/Data.hpp"
#include "blockchain/protocol/bitcoin/base/block/input/InputPrivate.hpp"
#include "internal/blockchain/protocol/bitcoin/base/block/Types.hpp"
#include "internal/util/PMR.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/block/Outpoint.hpp"
#include "opentxs/blockchain/block/Types.internal.hpp"
#include "opentxs/blockchain/cfilter/Types.hpp"
#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/blockchain/protocol/bitcoin/base/block/Input.hpp"
#include "opentxs/blockchain/protocol/bitcoin/base/block/Output.hpp"
#include "opentxs/blockchain/protocol/bitcoin/base/block/Script.hpp"
#include "opentxs/blockchain/protocol/bitcoin/base/block/Types.hpp"
#include "opentxs/core/Amount.hpp"
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/identifier/Generic.hpp"
#include "opentxs/identifier/Nym.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Numbers.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs
{
namespace api
{
namespace crypto
{
class Blockchain;
}  // namespace crypto

namespace session
{
class Client;
}  // namespace session

class Crypto;
class Session;
}  // namespace api

namespace blockchain
{
namespace block
{
class TransactionHash;
}  // namespace block

namespace protocol
{
namespace bitcoin
{
namespace base
{
namespace block
{
namespace internal
{
class Input;
}  // namespace internal
}  // namespace block
}  // namespace base
}  // namespace bitcoin
}  // namespace protocol
}  // namespace blockchain

class Writer;
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::protocol::bitcoin::base::block::implementation
{
class Input final : public InputPrivate
{
public:
    using PubkeyHashes = Set<ElementHash>;

    static const VersionNumber default_version_;

    auto AssociatedLocalNyms(
        const api::crypto::Blockchain& crypto,
        Set<identifier::Nym>& output) const noexcept -> void final;
    auto AssociatedRemoteContacts(
        const api::session::Client& api,
        Set<identifier::Generic>& output) const noexcept -> void final;
    auto CalculateSize(const bool normalized) const noexcept
        -> std::size_t final;
    auto Coinbase() const noexcept -> ReadView final;
    [[nodiscard]] auto clone(allocator_type alloc) const noexcept
        -> InputPrivate* final
    {
        return pmr::clone_as<InputPrivate>(this, {alloc});
    }
    auto ExtractElements(const cfilter::Type style, Elements& out)
        const noexcept -> void final;
    auto ExtractElements(const cfilter::Type style, alloc::Default alloc)
        const noexcept -> Elements;
    auto FindMatches(
        const api::Session& api,
        const TransactionHash& txid,
        const cfilter::Type type,
        const Patterns& txos,
        const ParsedPatterns& elements,
        const std::size_t position,
        const Log& log,
        Matches& out,
        alloc::Default monotonic) const noexcept -> void final;
    auto GetBytes(std::size_t& base, std::size_t& witness) const noexcept
        -> void final;
    auto IsValid() const noexcept -> bool final { return true; }
    auto Keys(alloc::Default alloc) const noexcept -> Set<crypto::Key> final;
    auto Keys(Set<crypto::Key>& out) const noexcept -> void final;
    auto IndexElements(const api::Session& api, ElementHashes& out)
        const noexcept -> void final;
    auto NetBalanceChange(
        const api::crypto::Blockchain& crypto,
        const identifier::Nym& nym,
        const std::size_t index,
        const Log& log) const noexcept -> opentxs::Amount final;
    auto Payer() const noexcept -> identifier::Generic
    {
        return cache_.lock()->payer();
    }
    auto PreviousOutput() const noexcept -> const Outpoint& final
    {
        return previous_;
    }
    auto Print(const api::Crypto& crypto) const noexcept
        -> UnallocatedCString final;
    auto Print(const api::Crypto& crypto, alloc::Default alloc) const noexcept
        -> CString final;
    auto Serialize(Writer&& destination) const noexcept
        -> std::optional<std::size_t> final;
    auto SerializeNormalized(Writer&& destination) const noexcept
        -> std::optional<std::size_t> final;
    auto Serialize(
        const api::Session& api,
        const std::uint32_t index,
        SerializeType& destination) const noexcept -> bool final;
    auto SignatureVersion(alloc::Default alloc) const noexcept
        -> block::Input final;
    auto SignatureVersion(block::Script subscript, alloc::Default alloc)
        const noexcept -> block::Input final;
    auto Script() const noexcept -> const block::Script& final
    {
        return script_;
    }
    auto Sequence() const noexcept -> std::uint32_t final { return sequence_; }
    auto Spends() const noexcept(false) -> const block::Output& final
    {
        return cache_.lock()->spends();
    }
    auto Witness() const noexcept -> std::span<const WitnessItem> final
    {
        return witness_;
    }

    auto AddMultisigSignatures(const Signatures& signatures) noexcept
        -> bool final;
    auto AddSignatures(const Signatures& signatures) noexcept -> bool final;
    auto AssociatePreviousOutput(const block::Output& output) noexcept
        -> bool final;
    auto ConfirmMatches(
        const Log& log,
        const api::crypto::Blockchain& api,
        const Matches& candiates) noexcept -> bool final;
    [[nodiscard]] auto get_deleter() noexcept -> delete_function final
    {
        return pmr::make_deleter(this);
    }
    auto MergeMetadata(
        const api::Crypto& crypto,
        const internal::Input& rhs,
        const std::size_t index,
        const Log& log) noexcept -> void final;
    auto RefreshContacts(const api::crypto::Blockchain& api) noexcept
        -> void final;
    auto ReplaceScript() noexcept -> bool final;
    auto SetKeyData(const KeyData& data) noexcept -> void final
    {
        return cache_.lock()->set(data);
    }

    Input(
        const blockchain::Type chain,
        const std::uint32_t sequence,
        Outpoint&& previous,
        Vector<WitnessItem> witness,
        block::Script script,
        const VersionNumber version,
        std::optional<std::size_t> size,
        allocator_type alloc) noexcept(false);
    Input(
        const blockchain::Type chain,
        const std::uint32_t sequence,
        Outpoint&& previous,
        Vector<WitnessItem> witness,
        block::Script script,
        const VersionNumber version,
        block::Output output,
        Set<crypto::Key>&& keys,
        allocator_type alloc) noexcept(false);
    Input(
        const blockchain::Type chain,
        const std::uint32_t sequence,
        Outpoint&& previous,
        Vector<WitnessItem> witness,
        const ReadView coinbase,
        const VersionNumber version,
        block::Output output,
        std::optional<std::size_t> size,
        allocator_type alloc) noexcept(false);
    Input(
        const blockchain::Type chain,
        const std::uint32_t sequence,
        Outpoint&& previous,
        Vector<WitnessItem> witness,
        block::Script script,
        ByteArray coinbase,
        const VersionNumber version,
        std::optional<std::size_t> size,
        Set<crypto::Key>&& keys,
        block::Output output,
        allocator_type alloc) noexcept(false);
    Input() = delete;
    Input(const Input& rhs, allocator_type alloc) noexcept;
    Input(
        const Input& rhs,
        block::Script script,
        allocator_type alloc) noexcept;
    Input(const Input&) = delete;
    Input(Input&&) = delete;
    auto operator=(const Input&) -> Input& = delete;
    auto operator=(Input&&) -> Input& = delete;

    ~Input() final;

private:
    static const VersionNumber outpoint_version_;
    static const VersionNumber key_version_;
    static const VersionNumber witness_version_;

    enum class Redeem : std::uint8_t {
        None,
        MaybeP2WSH,
        P2SH_P2WSH,
        P2SH_P2WPKH,
    };

    const blockchain::Type chain_;
    const VersionNumber serialize_version_;
    const Outpoint previous_;
    Vector<WitnessItem> witness_;
    block::Script script_;
    const ByteArray coinbase_;
    const std::uint32_t sequence_;
    mutable libguarded::plain_guarded<input::Data> cache_;

    static auto refresh_contacts(
        const api::crypto::Blockchain& api,
        input::Data& cache,
        allocator_type alloc) noexcept -> void;

    auto classify() const noexcept -> Redeem;
    auto decode_coinbase() const noexcept -> UnallocatedCString;
    auto get_pubkeys(const api::Session& api, alloc::Default monotonic)
        const noexcept -> const PubkeyHashes&;
    auto get_script_hash(const api::Session& api) const noexcept
        -> const std::optional<ElementHash>&;
    auto index_elements(
        const api::Session& api,
        PubkeyHashes& hashes,
        alloc::Default monotonic) const noexcept -> void;
    auto is_bip16() const noexcept;
    auto payload_bytes() const noexcept -> std::size_t;
    auto serialize(Writer&& destination, const bool normalized) const noexcept
        -> std::optional<std::size_t>;
};
}  // namespace
   // opentxs::blockchain::protocol::bitcoin::base::block::implementation
