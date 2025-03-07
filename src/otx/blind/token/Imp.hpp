// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdint>

#include "internal/otx/blind/Purse.hpp"
#include "opentxs/Time.hpp"
#include "opentxs/core/Amount.hpp"
#include "opentxs/crypto/symmetric/Types.hpp"
#include "opentxs/identifier/Notary.hpp"
#include "opentxs/identifier/UnitDefinition.hpp"
#include "opentxs/otx/blind/Token.hpp"
#include "opentxs/otx/blind/Types.hpp"
#include "opentxs/util/Numbers.hpp"
#include "otx/blind/token/Token.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs
{
namespace api
{
class Session;
}  // namespace api

namespace crypto
{
namespace symmetric
{
class Key;
}  // namespace symmetric
}  // namespace crypto

namespace identity
{
class Nym;
}  // namespace identity

namespace otx
{
namespace blind
{
class Mint;
}  // namespace blind
}  // namespace otx

namespace protobuf
{
class Ciphertext;
class Token;
}  // namespace protobuf

class PasswordPrompt;
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

#define OT_TOKEN_VERSION 1

namespace opentxs::otx::blind::token
{
class Token : virtual public blind::Token::Imp
{
public:
    auto IsValid() const noexcept -> bool final { return true; }
    auto Notary() const -> const identifier::Notary& override
    {
        return notary_;
    }
    auto Owner() const noexcept -> blind::internal::Purse& final
    {
        return purse_;
    }
    auto Series() const -> MintSeries override { return series_; }
    auto State() const -> blind::TokenState override { return state_; }
    auto Type() const -> blind::CashType override { return type_; }
    auto Unit() const -> const identifier::UnitDefinition& override
    {
        return unit_;
    }
    auto ValidFrom() const -> Time override { return valid_from_; }
    auto ValidTo() const -> Time override { return valid_to_; }
    auto Value() const -> Denomination override { return denomination_; }

    virtual auto GenerateTokenRequest(
        const identity::Nym& owner,
        const Mint& mint,
        const PasswordPrompt& reason) -> bool = 0;

    Token() = delete;
    Token(Token&&) = delete;
    auto operator=(const Token&) -> Token& = delete;
    auto operator=(Token&&) -> Token& = delete;

    ~Token() override = default;

protected:
    static const opentxs::crypto::symmetric::Algorithm mode_;

    const api::Session& api_;
    blind::internal::Purse& purse_;
    blind::TokenState state_;
    const identifier::Notary notary_;
    const identifier::UnitDefinition unit_;
    const std::uint64_t series_;
    const Denomination denomination_;
    const Time valid_from_;
    const Time valid_to_;

    auto reencrypt(
        const crypto::symmetric::Key& oldKey,
        const PasswordPrompt& oldPassword,
        const crypto::symmetric::Key& newKey,
        const PasswordPrompt& newPassword,
        protobuf::Ciphertext& ciphertext) -> bool;

    auto Serialize(protobuf::Token& out) const noexcept -> bool override;

    Token(
        const api::Session& api,
        blind::internal::Purse& purse,
        const protobuf::Token& serialized);
    Token(
        const api::Session& api,
        blind::internal::Purse& purse,
        const VersionNumber version,
        const blind::TokenState state,
        const std::uint64_t series,
        const Denomination denomination,
        const Time validFrom,
        const Time validTo);
    Token(const Token&);

private:
    const blind::CashType type_;
    const VersionNumber version_;

    Token(
        const api::Session& api,
        blind::internal::Purse& purse,
        const blind::TokenState state,
        const blind::CashType type,
        const identifier::Notary& notary,
        const identifier::UnitDefinition& unit,
        const std::uint64_t series,
        const Denomination denomination,
        const Time validFrom,
        const Time validTo,
        const VersionNumber version);
};
}  // namespace opentxs::otx::blind::token
