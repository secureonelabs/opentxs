// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "internal/blockchain/protocol/bitcoin/base/block/Types.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/block/Types.internal.hpp"
#include "opentxs/blockchain/cfilter/Types.hpp"
#include "opentxs/blockchain/protocol/bitcoin/base/block/Script.hpp"
#include "opentxs/blockchain/protocol/bitcoin/base/block/script/Element.hpp"
#include "opentxs/blockchain/protocol/bitcoin/base/block/script/Types.hpp"
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/core/PaymentCode.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/Container.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs
{
namespace api
{
class Crypto;
class Session;
}  // namespace api

class Writer;
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::protocol::bitcoin::base::block::internal
{
class Script
{
public:
    static auto blank_signature(const blockchain::Type chain) noexcept
        -> const Space&;
    static auto blank_pubkey(
        const blockchain::Type chain,
        const bool compressed = true) noexcept -> const Space&;

    virtual auto CalculateHash160(const api::Crypto& crypto, Writer&& output)
        const noexcept -> bool;
    virtual auto CalculateSize() const noexcept -> std::size_t;
    virtual auto ExtractElements(const cfilter::Type style, Elements& out)
        const noexcept -> void;
    virtual auto get() const noexcept -> std::span<const script::Element>;
    virtual auto IndexElements(const api::Session& api, ElementHashes& out)
        const noexcept -> void;
    virtual auto IsNotification(
        const std::uint8_t version,
        const PaymentCode& recipient) const noexcept -> bool;
    virtual auto IsValid() const noexcept -> bool;
    virtual auto LikelyPubkeyHashes(const api::Crypto& crypto) const noexcept
        -> UnallocatedVector<ByteArray>;
    virtual auto M() const noexcept -> std::optional<std::uint8_t>;
    virtual auto MultisigPubkey(const std::size_t position) const noexcept
        -> std::optional<ReadView>;
    virtual auto N() const noexcept -> std::optional<std::uint8_t>;
    virtual auto Print() const noexcept -> UnallocatedCString;
    virtual auto Print(alloc::Default alloc) const noexcept -> CString;
    virtual auto Pubkey() const noexcept -> std::optional<ReadView>;
    virtual auto PubkeyHash() const noexcept -> std::optional<ReadView>;
    virtual auto RedeemScript(alloc::Default alloc) const noexcept
        -> block::Script;
    virtual auto Role() const noexcept -> script::Position;
    virtual auto ScriptHash() const noexcept -> std::optional<ReadView>;
    virtual auto Serialize(Writer&& destination) const noexcept -> bool;
    virtual auto SigningSubscript(
        const blockchain::Type chain,
        alloc::Default alloc) const noexcept -> block::Script;
    virtual auto Type() const noexcept -> script::Pattern;
    virtual auto Value(const std::size_t position) const noexcept
        -> std::optional<ReadView>;

    virtual ~Script() = default;
};
}  // namespace opentxs::blockchain::protocol::bitcoin::base::block::internal
