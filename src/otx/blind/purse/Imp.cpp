// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "otx/blind/purse/Imp.hpp"  // IWYU pragma: associated

#include <opentxs/protobuf/Envelope.pb.h>
#include <opentxs/protobuf/Purse.pb.h>
#include <opentxs/protobuf/Token.pb.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>  // IWYU pragma: keep
#include <utility>

#include "internal/api/crypto/Symmetric.hpp"
#include "internal/core/Factory.hpp"
#include "internal/crypto/symmetric/Key.hpp"
#include "internal/otx/blind/Factory.hpp"
#include "internal/otx/blind/Purse.hpp"
#include "internal/otx/blind/Token.hpp"
#include "internal/otx/consensus/Server.hpp"
#include "internal/util/PasswordPrompt.hpp"
#include "internal/util/Pimpl.hpp"
#include "opentxs/Time.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/api/Factory.internal.hpp"
#include "opentxs/api/Session.hpp"
#include "opentxs/api/crypto/Symmetric.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Factory.internal.hpp"
#include "opentxs/api/session/Notary.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/Secret.hpp"
#include "opentxs/crypto/symmetric/Algorithm.hpp"  // IWYU pragma: keep
#include "opentxs/crypto/symmetric/Key.hpp"
#include "opentxs/crypto/symmetric/Types.hpp"
#include "opentxs/identifier/Notary.hpp"
#include "opentxs/identifier/UnitDefinition.hpp"
#include "opentxs/identity/Nym.hpp"
#include "opentxs/otx/blind/Mint.hpp"
#include "opentxs/otx/blind/Purse.hpp"
#include "opentxs/otx/blind/PurseType.hpp"  // IWYU pragma: keep
#include "opentxs/otx/blind/Token.hpp"
#include "opentxs/otx/blind/TokenState.hpp"  // IWYU pragma: keep
#include "opentxs/otx/blind/Types.hpp"
#include "opentxs/otx/blind/Types.internal.hpp"
#include "opentxs/protobuf/Types.internal.hpp"
#include "opentxs/protobuf/Types.internal.tpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/PasswordPrompt.hpp"
#include "opentxs/util/Writer.hpp"

#define OT_PURSE_VERSION 1

namespace opentxs::factory
{
auto Purse(
    const api::Session& api,
    const otx::context::Server& context,
    const otx::blind::CashType type,
    const otx::blind::Mint& mint,
    const opentxs::Amount& totalValue,
    const opentxs::PasswordPrompt& reason) noexcept -> otx::blind::Purse
{
    return Purse(
        api,
        *context.Signer(),
        context.Notary(),
        context.RemoteNym(),
        type,
        mint,
        totalValue,
        reason);
}

auto Purse(
    const api::Session& api,
    const identity::Nym& nym,
    const identifier::Notary& server,
    const identity::Nym& serverNym,
    const otx::blind::CashType type,
    const otx::blind::Mint& mint,
    const opentxs::Amount& totalValue,
    const opentxs::PasswordPrompt& reason) noexcept -> otx::blind::Purse
{
    using ReturnType = otx::blind::Purse;
    using Imp = otx::blind::purse::Purse;
    auto pEnvelope = std::make_unique<OTEnvelope>(
        api.Factory().Internal().Session().Envelope());

    assert_false(nullptr == pEnvelope);

    auto& envelope = pEnvelope->get();
    auto pSecondaryPassword = api.Factory().Secret(0);
    auto& secondaryPassword = pSecondaryPassword;
    secondaryPassword.Randomize(32);
    auto password = api.Factory().PasswordPrompt(reason);
    password.Internal().SetPassword(secondaryPassword);
    auto pSecondaryKey = std::make_unique<crypto::symmetric::Key>(
        api.Crypto().Symmetric().Key(password));

    assert_false(nullptr == pSecondaryKey);

    auto locked = envelope.Seal(nym, secondaryPassword.Bytes(), reason);

    if (false == locked) { return {}; }

    auto output = std::make_unique<Imp>(
        api,
        nym.ID(),
        server,
        type,
        mint,
        std::move(pSecondaryPassword),
        std::move(pSecondaryKey),
        std::move(pEnvelope));

    if (!output) { return {}; }

    auto* imp = output.get();
    auto& internal = *imp;
    locked = internal.AddNym(serverNym, reason);

    if (false == locked) { return {}; }

    locked = internal.AddNym(nym, reason);

    if (false == locked) { return {}; }

    auto purse = ReturnType{output.release()};

    const auto generated =
        internal.GeneratePrototokens(nym, mint, totalValue, reason);

    if (false == generated) { return {}; }

    return purse;
}

auto Purse(const api::Session& api, const protobuf::Purse& in) noexcept
    -> otx::blind::Purse
{
    using ReturnType = otx::blind::Purse;
    using Imp = otx::blind::purse::Purse;
    auto output = std::make_unique<Imp>(api, in);
    auto* imp = output.get();
    auto& internal = *imp;
    auto purse = ReturnType(output.release());
    internal.DeserializeTokens(in);

    return purse;
}

auto Purse(const api::Session& api, const ReadView& in) noexcept
    -> otx::blind::Purse
{
    return Purse(api, opentxs::protobuf::Factory<protobuf::Purse>(in));
}

auto Purse(
    const api::Session& api,
    const otx::blind::Purse& request,
    const identity::Nym& requester,
    const opentxs::PasswordPrompt& reason) noexcept -> otx::blind::Purse
{
    using Imp = otx::blind::purse::Purse;
    const auto* rhs = dynamic_cast<const Imp*>(&(request.Internal()));

    if (nullptr == rhs) {
        LogError()()("invalid input purse").Flush();

        return {};
    }

    auto output = std::make_unique<Imp>(api, *rhs);

    assert_false(nullptr == output);

    auto& purse = *output;
    auto locked = purse.AddNym(requester, reason);

    if (false == locked) { return {}; }

    return output.release();
}

auto Purse(
    const api::Session& api,
    const identity::Nym& owner,
    const identifier::Notary& server,
    const identifier::UnitDefinition& unit,
    const otx::blind::CashType type,
    const opentxs::PasswordPrompt& reason) noexcept -> otx::blind::Purse
{
    using Imp = otx::blind::purse::Purse;
    auto output = std::make_unique<Imp>(api, server, unit, type);

    assert_false(nullptr == output);

    const auto added = output->AddNym(owner, reason);

    if (false == added) {
        LogError()()("Failed to encrypt purse").Flush();

        return {};
    }

    return output.release();
}
}  // namespace opentxs::factory

namespace opentxs::otx::blind::purse
{
const opentxs::crypto::symmetric::Algorithm Purse::mode_{
    opentxs::crypto::symmetric::Algorithm::ChaCha20Poly1305};

Purse::Purse(
    const api::Session& api,
    const VersionNumber version,
    const blind::CashType type,
    const identifier::Notary& notary,
    const identifier::UnitDefinition& unit,
    const blind::PurseType state,
    const Amount& totalValue,
    const Time validFrom,
    const Time validTo,
    const UnallocatedVector<blind::Token>& tokens,
    const std::shared_ptr<crypto::symmetric::Key> primary,
    const UnallocatedVector<protobuf::Envelope>& primaryPasswords,
    const std::shared_ptr<const crypto::symmetric::Key> secondaryKey,
    const std::shared_ptr<const OTEnvelope> secondaryEncrypted,
    std::optional<Secret> secondaryKeyPassword) noexcept
    : api_(api)
    , version_(version)
    , type_(type)
    , notary_(notary)
    , unit_(unit)
    , state_(state)
    , total_value_(totalValue)
    , latest_valid_from_(validFrom)
    , earliest_valid_to_(validTo)
    , tokens_(tokens)
    , unlocked_(false)
    , primary_key_password_(api_.Factory().Secret(0))
    , primary_(primary)
    , primary_passwords_(primaryPasswords)
    , secondary_key_password_(
          secondaryKeyPassword.has_value()
              ? std::move(secondaryKeyPassword.value())
              : api_.Factory().Secret(0))
    , secondary_(std::move(secondaryKey))
    , secondary_password_(std::move(secondaryEncrypted))
{
}

Purse::Purse(const Purse& rhs) noexcept
    : Purse(
          rhs.api_,
          rhs.version_,
          rhs.type_,
          rhs.notary_,
          rhs.unit_,
          rhs.state_,
          rhs.total_value_,
          rhs.latest_valid_from_,
          rhs.earliest_valid_to_,
          rhs.tokens_,
          rhs.primary_,
          rhs.primary_passwords_,
          rhs.secondary_,
          rhs.secondary_password_,
          {})
{
}

Purse::Purse(
    const api::Session& api,
    const identifier::Nym& owner,
    const identifier::Notary& server,
    const blind::CashType type,
    const Mint& mint,
    Secret&& secondaryKeyPassword,
    std::unique_ptr<const crypto::symmetric::Key> secondaryKey,
    std::unique_ptr<const OTEnvelope> secondaryEncrypted) noexcept
    : Purse(
          api,
          OT_PURSE_VERSION,
          type,
          server,
          mint.InstrumentDefinitionID(),
          blind::PurseType::Request,
          0,
          Time::min(),
          Time::max(),
          {},
          nullptr,
          {},
          std::move(secondaryKey),
          std::move(secondaryEncrypted),
          std::move(secondaryKeyPassword))
{
    auto primary = generate_key(primary_key_password_);
    primary_.reset(new crypto::symmetric::Key(std::move(primary)));
    unlocked_ = true;

    assert_false(nullptr == primary_);
    assert_false(nullptr == secondary_);
    assert_false(nullptr == secondary_password_);
}

Purse::Purse(
    const api::Session& api,
    const identifier::Notary& server,
    const identifier::UnitDefinition& unit,
    const blind::CashType type) noexcept
    : Purse(
          api,
          OT_PURSE_VERSION,
          type,
          server,
          unit,
          blind::PurseType::Normal,
          0,
          Time::min(),
          Time::max(),
          {},
          nullptr,
          {},
          nullptr,
          nullptr,
          {})
{
    auto primary = generate_key(primary_key_password_);
    primary_.reset(new crypto::symmetric::Key(std::move(primary)));
    unlocked_ = true;

    assert_false(nullptr == primary_);
}

Purse::Purse(const api::Session& api, const protobuf::Purse& in) noexcept
    : Purse(
          api,
          in.version(),
          opentxs::translate(in.type()),
          api.Factory().NotaryIDFromBase58(in.notary()),
          api.Factory().UnitIDFromBase58(in.mint()),
          opentxs::translate(in.state()),
          factory::Amount(in.totalvalue()),
          seconds_since_epoch_unsigned(in.latestvalidfrom()).value(),
          seconds_since_epoch_unsigned(in.earliestvalidto()).value(),
          {},
          nullptr,
          get_passwords(in),
          deserialize_secondary_key(api, in),
          deserialize_secondary_password(api, in),
          {})
{
    auto primary = api.Crypto().Symmetric().InternalSymmetric().Key(
        in.primarykey(),
        opentxs::crypto::symmetric::Algorithm::ChaCha20Poly1305);
    primary_.reset(new crypto::symmetric::Key(std::move(primary)));

    assert_false(nullptr == primary_);
}

Purse::Purse(const api::Session& api, const Purse& owner) noexcept
    : Purse(
          api,
          owner.version_,
          owner.type_,
          owner.notary_,
          owner.unit_,
          blind::PurseType::Issue,
          0,
          Time::min(),
          Time::max(),
          {},
          nullptr,
          {},
          owner.secondary_,
          owner.secondary_password_,
          {})
{
    auto primary = generate_key(primary_key_password_);
    primary_.reset(new crypto::symmetric::Key(std::move(primary)));
    unlocked_ = true;

    assert_false(nullptr == primary_);
}

auto Purse::AddNym(const identity::Nym& nym, const PasswordPrompt& reason)
    -> bool
{
    if (false == unlocked_) {
        LogError()()("Purse is locked").Flush();

        return false;
    }

    primary_passwords_.emplace_back();
    auto& sessionKey = *primary_passwords_.rbegin();

    if (false == bool(primary_)) {
        LogError()()("Missing primary key").Flush();

        return false;
    }

    auto envelope = api_.Factory().Internal().Session().Envelope();

    if (envelope->Seal(nym, primary_key_password_.Bytes(), reason)) {
        if (false == envelope->Serialize(sessionKey)) { return false; }
    } else {
        LogError()()("Failed to add nym").Flush();
        primary_passwords_.pop_back();

        return false;
    }

    return true;
}

auto Purse::apply_times(const Token& token) -> void
{
    latest_valid_from_ = std::max(latest_valid_from_, token.ValidFrom());
    earliest_valid_to_ = std::min(earliest_valid_to_, token.ValidTo());
}

auto Purse::begin() noexcept -> iterator
{
    assert_false(nullptr == parent_);

    return {parent_, 0};
}

auto Purse::cbegin() const noexcept -> const_iterator
{
    assert_false(nullptr == parent_);

    return {parent_, 0};
}

auto Purse::DeserializeTokens(const protobuf::Purse& in) noexcept -> void
{
    for (const auto& serialized : in.token()) {
        tokens_.emplace_back(factory::Token(api_, *this, serialized));
    }
}

auto Purse::cend() const noexcept -> const_iterator
{
    assert_false(nullptr == parent_);

    return {parent_, tokens_.size()};
}

auto Purse::deserialize_secondary_key(
    const api::Session& api,
    const protobuf::Purse& in) noexcept(false)
    -> std::unique_ptr<const crypto::symmetric::Key>
{
    switch (opentxs::translate(in.state())) {
        case blind::PurseType::Request:
        case blind::PurseType::Issue: {
            auto output = std::make_unique<crypto::symmetric::Key>(
                api.Crypto().Symmetric().InternalSymmetric().Key(
                    in.secondarykey(),
                    opentxs::crypto::symmetric::Algorithm::ChaCha20Poly1305));

            if (false == bool(output)) {
                LogError()()("Invalid serialized secondary key").Flush();

                throw std::runtime_error("Invalid serialized secondary key");
            }

            return output;
        }
        case blind::PurseType::Normal: {
        } break;
        case blind::PurseType::Error:
        default: {
            LogError()()("Invalid purse state").Flush();

            throw std::runtime_error("invalid purse state");
        }
    }

    return {};
}

auto Purse::deserialize_secondary_password(
    const api::Session& api,
    const protobuf::Purse& in) noexcept(false)
    -> std::unique_ptr<const OTEnvelope>
{
    switch (opentxs::translate(in.state())) {
        case blind::PurseType::Request:
        case blind::PurseType::Issue: {
            auto output = std::make_unique<OTEnvelope>(
                api.Factory().Internal().Session().Envelope(
                    in.secondarypassword()));

            if (false == bool(output)) {
                LogError()()(": Invalid serialized secondary password").Flush();

                throw std::runtime_error(
                    "Invalid serialized secondary password");
            }

            return output;
        }
        case blind::PurseType::Normal: {
        } break;
        case blind::PurseType::Error:
        default: {
            LogError()()("Invalid purse state").Flush();

            throw std::runtime_error("invalid purse state");
        }
    }

    return {};
}

auto Purse::end() noexcept -> iterator
{
    assert_false(nullptr == parent_);

    return {parent_, tokens_.size()};
}

auto Purse::generate_key(Secret& password) const -> crypto::symmetric::Key
{
    password.Randomize(32);
    auto keyPassword = api_.Factory().PasswordPrompt("");
    keyPassword.Internal().SetPassword(password);

    return api_.Crypto().Symmetric().Key(mode_, keyPassword);
}

// TODO replace this algorithm with one that will ensure all spends up to and
// including the specified amount are possible
auto Purse::GeneratePrototokens(
    const identity::Nym& owner,
    const Mint& mint,
    const Amount& amount,
    const opentxs::PasswordPrompt& reason) -> bool
{
    Amount workingAmount(amount);
    Amount tokenAmount{mint.GetLargestDenomination(workingAmount)};

    while (tokenAmount > 0) {
        try {
            workingAmount -= tokenAmount;
            auto token =
                factory::Token(api_, owner, mint, tokenAmount, *this, reason);

            if (!token) {
                LogError()()(": Failed to generate prototoken").Flush();

                return {};
            }

            if (false == Push(std::move(token), reason)) { return false; }

            tokenAmount = mint.GetLargestDenomination(workingAmount);
        } catch (const std::exception& e) {
            LogError()()(e.what()).Flush();

            return false;
        }
    }

    return total_value_ == amount;
}

auto Purse::get_passwords(const protobuf::Purse& in)
    -> UnallocatedVector<protobuf::Envelope>
{
    auto output = UnallocatedVector<protobuf::Envelope>{};

    for (const auto& password : in.primarypassword()) {
        output.emplace_back(password);
    }

    return output;
}

auto Purse::PrimaryKey(PasswordPrompt& password) -> crypto::symmetric::Key&
{
    if (false == bool(primary_)) { throw std::out_of_range("No primary key"); }

    if (primary_passwords_.empty()) {
        throw std::out_of_range("No session keys");
    }

    if (false == unlocked_) { throw std::out_of_range("Purse is locked"); }

    password.Internal().SetPassword(primary_key_password_);

    return *primary_;
}

auto Purse::Pop() -> Token
{
    if (0 == tokens_.size()) {
        LogTrace()()("Purse is empty").Flush();

        return {};
    }

    auto token = Token{std::move(tokens_.back())};
    tokens_.pop_back();
    total_value_ -= token.Value();
    recalculate_times();

    return token;
}

auto Purse::Process(
    const identity::Nym& owner,
    const Mint& mint,
    const opentxs::PasswordPrompt& reason) -> bool
{
    if (blind::PurseType::Issue != state_) {
        LogError()()("Incorrect purse state").Flush();

        return false;
    }

    bool processed{true};

    for (auto& token : tokens_) {
        processed &= token.Internal().Process(owner, mint, reason);
    }

    if (processed) {
        state_ = blind::PurseType::Normal;
        const_cast<std::shared_ptr<const OTEnvelope>&>(secondary_password_)
            .reset();
        const_cast<std::shared_ptr<const crypto::symmetric::Key>&>(secondary_)
            .reset();
        secondary_key_password_ = api_.Factory().Secret(0);
    } else {
        LogError()()("Failed to process token").Flush();
    }

    return processed;
}

auto Purse::Push(Token&& original, const opentxs::PasswordPrompt& reason)
    -> bool
{
    if (false == bool(original)) {
        LogError()()("Invalid token").Flush();

        return false;
    }

    if (false == bool(primary_)) {
        LogError()()("Missing primary key").Flush();

        return false;
    }

    if (false == unlocked_) {
        LogError()()("Purse is locked").Flush();

        return false;
    }

    auto copy = factory::Token(original, *this);

    assert_true(copy);

    if (false == copy.Internal().ChangeOwner(
                     original.Internal().Owner(), *this, reason)) {
        LogError()()("Failed to encrypt token").Flush();

        return false;
    }

    switch (copy.State()) {
        case blind::TokenState::Blinded:
        case blind::TokenState::Signed:
        case blind::TokenState::Ready: {
            total_value_ += copy.Value();
            apply_times(copy);
        } break;
        case blind::TokenState::Error:
        case blind::TokenState::Spent:
        case blind::TokenState::Expired:
        default: {
        }
    }

    tokens_.emplace(tokens_.begin(), std::move(copy));

    return true;
}

// TODO let's do this in constant time someday
void Purse::recalculate_times()
{
    latest_valid_from_ = Time::min();
    earliest_valid_to_ = Time::max();

    for (const auto& token : tokens_) { apply_times(token); }
}

auto Purse::SecondaryKey(
    const identity::Nym& owner,
    PasswordPrompt& passwordOut) -> const crypto::symmetric::Key&
{
    if (false == bool(secondary_)) {
        throw std::out_of_range("No secondary key");
    }

    if (false == bool(secondary_password_)) {
        throw std::out_of_range("No secondary key password");
    }

    const auto& secondaryKey = *secondary_;
    const auto& envelope = secondary_password_->get();
    const auto decrypted = envelope.Open(
        owner,
        secondary_key_password_.WriteInto(Secret::Mode::Mem),
        passwordOut);

    if (false == decrypted) {
        throw std::out_of_range("Failed to decrypt key password");
    }

    passwordOut.Internal().SetPassword(secondary_key_password_);
    const auto unlocked = secondaryKey.Unlock(passwordOut);

    if (false == unlocked) {
        throw std::out_of_range("Failed to unlock key password");
    }

    return secondaryKey;
}

auto Purse::Serialize(protobuf::Purse& output) const noexcept -> bool
{
    try {
        output.set_version(version_);
        output.set_type(opentxs::translate(type_));
        output.set_state(opentxs::translate(state_));
        output.set_notary(notary_.asBase58(api_.Crypto()));
        output.set_mint(unit_.asBase58(api_.Crypto()));
        total_value_.Serialize(writer(output.mutable_totalvalue()));
        output.set_latestvalidfrom(
            seconds_since_epoch(latest_valid_from_).value());
        output.set_earliestvalidto(
            seconds_since_epoch(earliest_valid_to_).value());

        for (const auto& token : tokens_) {
            token.Internal().Serialize(*output.add_token());
        }

        if (false == bool(primary_)) {
            throw std::runtime_error("missing primary key");
        }

        if (false ==
            primary_->Internal().Serialize(*output.mutable_primarykey())) {
            throw std::runtime_error("failed to serialize primary key");
        }

        for (const auto& password : primary_passwords_) {
            *output.add_primarypassword() = password;
        }

        switch (state_) {
            case blind::PurseType::Request:
            case blind::PurseType::Issue: {
                if (false == bool(secondary_)) {
                    throw std::runtime_error("missing secondary key");
                }

                if (!secondary_->Internal().Serialize(
                        *output.mutable_secondarykey())) {
                    throw std::runtime_error(
                        "failed to serialize secondary key");
                }

                if (false == bool(secondary_password_)) {
                    throw std::runtime_error("missing secondary password");
                }

                if (false == secondary_password_->get().Serialize(
                                 *output.mutable_secondarypassword())) {
                    throw std::runtime_error(
                        "faile to serialize secondary password");
                }
            } break;
            case blind::PurseType::Normal: {
            } break;
            case blind::PurseType::Error:
            default: {
                throw std::runtime_error("invalid purse state");
            }
        }
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return false;
    }

    return true;
}

auto Purse::Serialize(Writer&& destination) const noexcept -> bool
{
    auto proto = protobuf::Purse{};

    if (Serialize(proto)) { return write(proto, std::move(destination)); }

    return false;
}

auto Purse::Unlock(
    const identity::Nym& nym,
    const opentxs::PasswordPrompt& reason) const -> bool
{
    if (primary_passwords_.empty()) {
        LogError()()("No session keys found").Flush();

        return false;
    }

    if (false == bool(primary_)) {
        LogError()()("Missing primary key").Flush();

        return false;
    }

    const auto primary = *primary_;
    auto password = api_.Factory().Secret(0);

    for (const auto& sessionKey : primary_passwords_) {
        try {
            const auto envelope =
                api_.Factory().Internal().Session().Envelope(sessionKey);
            const auto opened = envelope->Open(
                nym, password.WriteInto(Secret::Mode::Mem), reason);

            if (opened) {
                auto unlocker =
                    api_.Factory().PasswordPrompt(reason.GetDisplayString());
                unlocker.Internal().SetPassword(password);
                unlocked_ = primary.Unlock(unlocker);

                if (unlocked_) {
                    primary_key_password_ = password;
                    break;
                } else {
                    LogError()()(
                        "Decrypted password does not unlock the primary key")
                        .Flush();
                }
            }
        } catch (...) {
            LogError()()("Invalid session key").Flush();

            continue;
        }
    }

    if (false == unlocked_) {
        LogError()()("Nym ")(nym.ID(), api_.Crypto())(
            " can not decrypt any session key in the purse.")
            .Flush();
    }

    return unlocked_;
}

auto Purse::Verify(const api::session::Notary& server) const -> bool
{
    Amount total{0};
    auto validFrom{Time::min()};
    auto validTo{Time::max()};
    UnallocatedSet<blind::TokenState> allowedStates{};

    switch (state_) {
        case blind::PurseType::Request: {
            allowedStates.insert(blind::TokenState::Blinded);
        } break;
        case blind::PurseType::Issue: {
            allowedStates.insert(blind::TokenState::Signed);
        } break;
        case blind::PurseType::Normal: {
            allowedStates.insert(blind::TokenState::Ready);
            allowedStates.insert(blind::TokenState::Spent);
            allowedStates.insert(blind::TokenState::Expired);
        } break;
        case blind::PurseType::Error:
        default: {
            LogError()()("Invalid purse state.").Flush();

            return false;
        }
    }

    for (const auto& token : tokens_) {
        if (type_ != token.Type()) {
            LogError()()("Token type does not match purse type.").Flush();

            return false;
        }

        if (notary_ != token.Notary()) {
            LogError()()("Token notary does not match purse notary.").Flush();

            return false;
        }

        if (unit_ != token.Unit()) {
            LogError()()("Token unit does not match purse unit.").Flush();

            return false;
        }

        if (false == allowedStates.contains(token.State())) {
            LogError()()("Incorrect token state").Flush();

            return false;
        }

        const auto series = token.Series();

        if (std::numeric_limits<std::uint32_t>::max() < series) {
            LogError()()("Invalid series").Flush();

            return false;
        }

        auto& mint =
            server.GetPrivateMint(unit_, static_cast<std::uint32_t>(series));

        if (!mint) {
            LogError()()("Incorrect token series").Flush();

            return false;
        }

        if (mint.Expired() && (token.State() != blind::TokenState::Expired)) {
            LogError()()("Token is expired").Flush();

            return false;
        }

        if (token.ValidFrom() != mint.GetValidFrom()) {
            LogError()()("Incorrect token valid from").Flush();

            return false;
        }

        if (token.ValidTo() != mint.GetValidTo()) {
            LogError()()("Incorrect token valid to").Flush();

            return false;
        }

        validFrom = std::max(validFrom, token.ValidFrom());
        validTo = std::min(validTo, token.ValidFrom());

        switch (token.State()) {
            case blind::TokenState::Blinded:
            case blind::TokenState::Signed:
            case blind::TokenState::Ready: {
                total += token.Value();
                [[fallthrough]];
            }
            case blind::TokenState::Spent:
            case blind::TokenState::Expired: {
            } break;
            case blind::TokenState::Error:
            default: {
                LogError()()("Invalid token state").Flush();

                return false;
            }
        }
    }

    if (total_value_ != total) {
        LogError()()("Incorrect purse value").Flush();

        return false;
    }

    if (latest_valid_from_ != validFrom) {
        LogError()()("Incorrect purse latest valid from").Flush();

        return false;
    }

    if (earliest_valid_to_ != validTo) {
        LogError()()("Incorrect purse earliest valid to").Flush();

        return false;
    }

    return true;
}

Purse::~Purse() = default;
}  // namespace opentxs::otx::blind::purse
