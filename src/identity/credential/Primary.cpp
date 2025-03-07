// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "identity/credential/Primary.hpp"  // IWYU pragma: associated

#include <frozen/bits/algorithms.h>
#include <frozen/bits/elsa.h>
#include <opentxs/protobuf/Credential.pb.h>
#include <opentxs/protobuf/Enums.pb.h>
#include <opentxs/protobuf/HDPath.pb.h>
#include <opentxs/protobuf/MasterCredentialParameters.pb.h>
#include <opentxs/protobuf/Signature.pb.h>
#include <opentxs/protobuf/SourceProof.pb.h>
#include <functional>
#include <memory>
#include <stdexcept>

#include "identity/credential/Key.hpp"
#include "internal/crypto/asymmetric/Key.hpp"
#include "internal/crypto/key/Key.hpp"
#include "internal/crypto/key/Keypair.hpp"
#include "internal/identity/Source.hpp"
#include "internal/identity/credential/Credential.hpp"
#include "opentxs/api/Factory.internal.hpp"
#include "opentxs/api/Session.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/crypto/Parameters.hpp"
#include "opentxs/crypto/asymmetric/Key.hpp"
#include "opentxs/crypto/asymmetric/Mode.hpp"  // IWYU pragma: keep
#include "opentxs/crypto/asymmetric/Types.hpp"
#include "opentxs/identifier/Generic.hpp"
#include "opentxs/identity/CredentialRole.hpp"  // IWYU pragma: keep
#include "opentxs/identity/NymCapability.hpp"   // IWYU pragma: keep
#include "opentxs/identity/Source.hpp"
#include "opentxs/identity/SourceProofType.hpp"
#include "opentxs/identity/SourceType.hpp"  // IWYU pragma: keep
#include "opentxs/identity/Types.hpp"
#include "opentxs/internal.factory.hpp"
#include "opentxs/protobuf/syntax/Credential.hpp"  // IWYU pragma: keep
#include "opentxs/protobuf/syntax/Types.internal.tpp"
#include "opentxs/util/Log.hpp"

namespace opentxs
{
auto Factory::PrimaryCredential(
    const api::Session& api,
    identity::internal::Authority& parent,
    const identity::Source& source,
    const crypto::Parameters& parameters,
    const VersionNumber version,
    const opentxs::PasswordPrompt& reason)
    -> identity::credential::internal::Primary*
{
    using ReturnType = identity::credential::implementation::Primary;

    try {

        return new ReturnType(api, parent, source, parameters, version, reason);
    } catch (const std::exception& e) {
        LogError()()("Failed to create credential: ")(e.what()).Flush();

        return nullptr;
    }
}

auto Factory::PrimaryCredential(
    const api::Session& api,
    identity::internal::Authority& parent,
    const identity::Source& source,
    const protobuf::Credential& serialized)
    -> identity::credential::internal::Primary*
{
    using ReturnType = identity::credential::implementation::Primary;

    try {

        return new ReturnType(api, parent, source, serialized);
    } catch (const std::exception& e) {
        LogError()()("Failed to deserialize credential: ")(e.what()).Flush();

        return nullptr;
    }
}
}  // namespace opentxs

namespace opentxs::identity::credential::implementation
{
const VersionConversionMap Primary::credential_to_master_params_{
    {1, 1},
    {2, 1},
    {3, 1},
    {4, 1},
    {5, 1},
    {6, 2},
};

Primary::Primary(
    const api::Session& api,
    const identity::internal::Authority& parent,
    const identity::Source& source,
    const crypto::Parameters& params,
    const VersionNumber version,
    const opentxs::PasswordPrompt& reason) noexcept(false)
    : credential::implementation::Key(
          api,
          parent,
          source,
          params,
          version,
          identity::CredentialRole::MasterKey,
          reason,
          {},
          identity::SourceType::PubKey == params.SourceType())
    , source_proof_(source_proof(params))
{
    first_time_init(set_name_from_id_);
    init(*this, reason);
}

Primary::Primary(
    const api::Session& api,
    const identity::internal::Authority& parent,
    const identity::Source& source,
    const protobuf::Credential& serialized) noexcept(false)
    : credential::implementation::Key(api, parent, source, serialized, {})
    , source_proof_(serialized.masterdata().sourceproof())
{
    init_serialized();
}

auto Primary::hasCapability(const NymCapability& capability) const -> bool
{
    switch (capability) {
        case NymCapability::SIGN_CHILDCRED: {

            return signing_key_->CheckCapability(capability);
        }
        case NymCapability::SIGN_MESSAGE:
        case NymCapability::ENCRYPT_MESSAGE:
        case NymCapability::AUTHENTICATE_CONNECTION:
        default: {

            return false;
        }
    }
}

auto Primary::Path(protobuf::HDPath& output) const -> bool
{
    try {
        const auto found =
            signing_key_->GetPrivateKey().Internal().Path(output);

        if (found) { output.mutable_child()->RemoveLast(); }

        return found;
    } catch (...) {
        LogError()()("No private key.").Flush();

        return false;
    }
}

auto Primary::Path() const -> UnallocatedCString
{
    return signing_key_->GetPrivateKey().Internal().Path();
}

auto Primary::serialize(
    const SerializationModeFlag asPrivate,
    const SerializationSignatureFlag asSigned) const
    -> std::shared_ptr<internal::Base::SerializedType>
{
    auto output = Key::serialize(asPrivate, asSigned);

    assert_false(nullptr == output);

    auto& serialized = *output;
    serialized.set_role(
        opentxs::translate(identity::CredentialRole::MasterKey));
    auto& masterData = *serialized.mutable_masterdata();
    masterData.set_version(credential_to_master_params_.at(Version()));
    if (false == source_.Internal().Serialize(*masterData.mutable_source())) {
        throw std::runtime_error("Failed to serialize source.");
    }
    *masterData.mutable_sourceproof() = source_proof_;

    return output;
}

void Primary::sign(
    const identity::credential::internal::Primary& master,
    const PasswordPrompt& reason,
    Signatures& out) noexcept(false)
{
    Key::sign(master, reason, out);

    if (protobuf::SOURCEPROOFTYPE_SELF_SIGNATURE != source_proof_.type()) {
        auto sig = std::make_shared<protobuf::Signature>();

        assert_false(nullptr == sig);

        if (false == source_.Internal().Sign(*this, *sig, reason)) {
            throw std::runtime_error("Failed to obtain source signature");
        }

        out.push_back(sig);
    }
}

auto Primary::source_proof(const crypto::Parameters& params)
    -> protobuf::SourceProof
{
    auto output = protobuf::SourceProof{};
    output.set_version(1);
    output.set_type(translate(params.SourceProofType()));

    return output;
}

auto Primary::sourceprooftype_map() noexcept -> const SourceProofTypeMap&
{
    using enum identity::SourceProofType;
    using enum protobuf::SourceProofType;
    static constexpr auto map = SourceProofTypeMap{
        {Error, SOURCEPROOFTYPE_ERROR},
        {SelfSignature, SOURCEPROOFTYPE_SELF_SIGNATURE},
        {Signature, SOURCEPROOFTYPE_SIGNATURE},
    };

    return map;
}

auto Primary::translate(const identity::SourceProofType in) noexcept
    -> protobuf::SourceProofType
{
    try {
        return sourceprooftype_map().at(in);
    } catch (...) {
        return protobuf::SOURCEPROOFTYPE_ERROR;
    }
}

auto Primary::translate(const protobuf::SourceProofType in) noexcept
    -> identity::SourceProofType
{
    static const auto map = frozen::invert_unordered_map(sourceprooftype_map());

    try {
        return map.at(in);
    } catch (...) {
        return identity::SourceProofType::Error;
    }
}

auto Primary::Verify(
    const protobuf::Credential& credential,
    const identity::CredentialRole& role,
    const identifier_type& masterID,
    const protobuf::Signature& masterSig) const -> bool
{
    if (!protobuf::syntax::check(
            LogError(),
            credential,
            opentxs::translate(crypto::asymmetric::Mode::Public),
            opentxs::translate(role),
            false)) {
        LogError()()("Invalid credential syntax.").Flush();

        return false;
    }

    const bool sameMaster = (ID() == masterID);

    if (!sameMaster) {
        LogError()()(
            "Credential does not designate this credential as its master.")
            .Flush();

        return false;
    }

    protobuf::Credential copy;
    copy.CopyFrom(credential);
    auto& signature = *copy.add_signature();
    signature.CopyFrom(masterSig);
    signature.clear_signature();

    return Verify(api_.Factory().Internal().Data(copy), masterSig);
}

auto Primary::verify_against_source() const -> bool
{
    auto pSerialized = std::shared_ptr<protobuf::Credential>{};
    auto hasSourceSignature{true};

    switch (source_.Type()) {
        case identity::SourceType::PubKey: {
            pSerialized = serialize(AS_PUBLIC, WITH_SIGNATURES);
            hasSourceSignature = false;
        } break;
        case identity::SourceType::Bip47: {
            pSerialized = serialize(AS_PUBLIC, WITHOUT_SIGNATURES);
        } break;
        case identity::SourceType::Error:
        default: {

            return false;
        }
    }

    if (false == bool(pSerialized)) {
        LogError()()("Failed to serialize credentials").Flush();

        return false;
    }

    const auto& serialized = *pSerialized;
    const auto pSig = hasSourceSignature ? SourceSignature() : SelfSignature();

    if (false == bool(pSig)) {
        LogError()()("Master credential not signed by its source.").Flush();

        return false;
    }

    const auto& sig = *pSig;

    return source_.Internal().Verify(serialized, sig);
}

auto Primary::verify_internally() const -> bool
{
    // Perform common Key Credential verifications
    if (!Key::verify_internally()) { return false; }

    // Check that the source validates this credential
    if (!verify_against_source()) {
        LogConsole()()("Failed verifying master credential against "
                       "nym id source.")
            .Flush();

        return false;
    }

    return true;
}
}  // namespace opentxs::identity::credential::implementation
