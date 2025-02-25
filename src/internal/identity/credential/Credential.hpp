// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <opentxs/protobuf/Enums.pb.h>
#include <memory>
#include <optional>

#include "internal/crypto/key/Keypair.hpp"
#include "internal/identity/credential/Types.hpp"
#include "opentxs/contract/Types.internal.hpp"
#include "opentxs/core/Secret.hpp"  // IWYU pragma: keep
#include "opentxs/crypto/HashType.hpp"
#include "opentxs/crypto/Types.hpp"
#include "opentxs/crypto/Types.internal.hpp"
#include "opentxs/crypto/asymmetric/Role.hpp"
#include "opentxs/identity/Types.hpp"
#include "opentxs/identity/Types.internal.hpp"
#include "opentxs/identity/credential/Base.hpp"
#include "opentxs/identity/credential/Contact.hpp"
#include "opentxs/identity/credential/Key.hpp"
#include "opentxs/identity/credential/Primary.hpp"
#include "opentxs/identity/credential/Secondary.hpp"
#include "opentxs/identity/credential/Verification.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs
{
namespace identity
{
namespace credential
{
namespace internal
{
struct Key;
}  // namespace internal
}  // namespace credential
}  // namespace identity

namespace protobuf
{
class ContactData;
class Credential;
class Signature;
class VerificationSet;
}  // namespace protobuf
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::identity::credential::internal
{
class Base : virtual public identity::credential::Base
{
public:
    using SerializedType = protobuf::Credential;

    virtual auto asKey() const noexcept -> const Key&;
    virtual auto GetContactData(protobuf::ContactData& contactData) const
        -> bool = 0;
    virtual auto GetVerificationSet(
        protobuf::VerificationSet& verificationSet) const -> bool = 0;
    auto Internal() const noexcept -> const internal::Base& final
    {
        return *this;
    }
    virtual auto MasterSignature() const -> contract::Signature = 0;
    virtual auto SelfSignature(CredentialModeFlag version = PUBLIC_VERSION)
        const -> contract::Signature = 0;
    using Signable::Serialize;
    virtual auto Serialize(
        SerializedType& serialized,
        const SerializationModeFlag asPrivate,
        const SerializationSignatureFlag asSigned) const -> bool = 0;
    virtual auto SourceSignature() const -> contract::Signature = 0;
    virtual auto Verify(
        const Data& plaintext,
        const protobuf::Signature& sig,
        const opentxs::crypto::asymmetric::Role key =
            opentxs::crypto::asymmetric::Role::Sign) const -> bool = 0;
    virtual auto Verify(
        const protobuf::Credential& credential,
        const identity::CredentialRole& role,
        const identifier::Generic& masterID,
        const protobuf::Signature& masterSig) const -> bool = 0;

    virtual auto asKey() noexcept -> Key&;
    auto Internal() noexcept -> internal::Base& final { return *this; }

#ifdef _MSC_VER
    Base() {}
#endif  // _MSC_VER
    ~Base() override = default;
};
struct Contact : virtual public Base,
                 virtual public identity::credential::Contact {
#ifdef _MSC_VER
    Contact() {}
#endif  // _MSC_VER
    ~Contact() override = default;
};
struct Key : virtual public Base, virtual public identity::credential::Key {
    static auto Blank() noexcept -> Key&;

    virtual auto GetKeypair(
        const crypto::asymmetric::Algorithm type,
        const opentxs::crypto::asymmetric::Role role) const
        -> const crypto::key::Keypair& = 0;
    virtual auto GetKeypair(const opentxs::crypto::asymmetric::Role role) const
        -> const crypto::key::Keypair& = 0;
    virtual auto GetPublicKeysBySignature(
        crypto::key::Keypair::Keys& listOutput,
        const opentxs::Signature& theSignature,
        char cKeyType = '0') const -> std::int32_t = 0;
    virtual auto Sign(
        const crypto::GetPreimage input,
        const crypto::SignatureRole role,
        protobuf::Signature& signature,
        const PasswordPrompt& reason,
        opentxs::crypto::asymmetric::Role key =
            opentxs::crypto::asymmetric::Role::Sign,
        const crypto::HashType hash = crypto::HashType::Error) const
        -> bool = 0;

#ifdef _MSC_VER
    Key() {}
#endif  // _MSC_VER
    ~Key() override = default;
};
struct Primary : virtual public Key,
                 virtual public identity::credential::Primary {
#ifdef _MSC_VER
    Primary() {}
#endif  // _MSC_VER
    ~Primary() override = default;
};
struct Secondary : virtual public Key,
                   virtual public identity::credential::Secondary {
#ifdef _MSC_VER
    Secondary() {}
#endif  // _MSC_VER
    ~Secondary() override = default;
};
struct Verification : virtual public Base,
                      virtual public identity::credential::Verification {
#ifdef _MSC_VER
    Verification() {}
#endif  // _MSC_VER
    ~Verification() override = default;
};
}  // namespace opentxs::identity::credential::internal

namespace opentxs
{
auto translate(const identity::CredentialRole in) noexcept
    -> protobuf::CredentialRole;
auto translate(const identity::CredentialType in) noexcept
    -> protobuf::CredentialType;
auto translate(const protobuf::CredentialRole in) noexcept
    -> identity::CredentialRole;
auto translate(const protobuf::CredentialType in) noexcept
    -> identity::CredentialType;
}  // namespace opentxs
