// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/crypto/asymmetric/Key.hpp"

#pragma once

#include <opentxs/protobuf/Enums.pb.h>
#include <cstdint>
#include <memory>
#include <utility>

#include "internal/core/String.hpp"
#include "internal/crypto/key/Keypair.hpp"
#include "internal/identity/Authority.hpp"
#include "internal/identity/credential/Credential.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/Types.internal.hpp"
#include "opentxs/crypto/Types.hpp"
#include "opentxs/crypto/Types.internal.hpp"
#include "opentxs/crypto/asymmetric/Types.hpp"
#include "opentxs/identifier/Generic.hpp"
#include "opentxs/identity/Nym.hpp"
#include "opentxs/identity/Source.hpp"
#include "opentxs/identity/Types.hpp"
#include "opentxs/identity/Types.internal.hpp"
#include "opentxs/identity/credential/Key.hpp"
#include "opentxs/identity/credential/Primary.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Numbers.hpp"

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

class Parameters;
}  // namespace crypto

namespace identity
{
namespace credential
{
class Base;
}  // namespace credential
}  // namespace identity

namespace protobuf
{
class ContactData;
class Credential;
class HDPath;
class Signature;
class VerificationItem;
class VerificationSet;
}  // namespace protobuf

class Data;
class Factory;
class PasswordPrompt;
class Secret;
class Signature;
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::identity::implementation
{
class Authority final : virtual public identity::internal::Authority
{
public:
    auto ContactCredentialVersion() const -> VersionNumber final
    {
        return authority_to_contact_.at(version_);
    }
    auto EncryptionTargets() const noexcept -> AuthorityKeys final;
    auto GetContactData(protobuf::ContactData& contactData) const -> bool final;
    auto GetMasterCredential() const -> const credential::Primary& final
    {
        return *master_;
    }
    auto GetMasterCredID() const -> identifier::Generic final;
    auto GetPublicAuthKey(
        crypto::asymmetric::Algorithm keytype,
        const String::List* plistRevokedIDs = nullptr) const
        -> const crypto::asymmetric::Key& final;
    auto GetPublicEncrKey(
        crypto::asymmetric::Algorithm keytype,
        const String::List* plistRevokedIDs = nullptr) const
        -> const crypto::asymmetric::Key& final;
    auto GetPublicKeysBySignature(
        crypto::key::Keypair::Keys& listOutput,
        const Signature& theSignature,
        char cKeyType = '0') const -> std::int32_t final;
    auto GetPublicSignKey(
        crypto::asymmetric::Algorithm keytype,
        const String::List* plistRevokedIDs = nullptr) const
        -> const crypto::asymmetric::Key& final;
    auto GetPrivateSignKey(
        crypto::asymmetric::Algorithm keytype,
        const String::List* plistRevokedIDs = nullptr) const
        -> const crypto::asymmetric::Key& final;
    auto GetPrivateEncrKey(
        crypto::asymmetric::Algorithm keytype,
        const String::List* plistRevokedIDs = nullptr) const
        -> const crypto::asymmetric::Key& final;
    auto GetPrivateAuthKey(
        crypto::asymmetric::Algorithm keytype,
        const String::List* plistRevokedIDs = nullptr) const
        -> const crypto::asymmetric::Key& final;
    auto GetAuthKeypair(
        crypto::asymmetric::Algorithm keytype,
        const String::List* plistRevokedIDs = nullptr) const
        -> const crypto::key::Keypair& final;
    auto GetEncrKeypair(
        crypto::asymmetric::Algorithm keytype,
        const String::List* plistRevokedIDs = nullptr) const
        -> const crypto::key::Keypair& final;
    auto GetSignKeypair(
        crypto::asymmetric::Algorithm keytype,
        const String::List* plistRevokedIDs = nullptr) const
        -> const crypto::key::Keypair& final;
    auto GetTagCredential(crypto::asymmetric::Algorithm keytype) const
        noexcept(false) -> const credential::Key& final;
    auto GetVerificationSet(protobuf::VerificationSet& verificationSet) const
        -> bool final;
    auto hasCapability(const NymCapability& capability) const -> bool final;
    auto Params(const crypto::asymmetric::Algorithm type) const noexcept
        -> ReadView final;
    auto Path(protobuf::HDPath& output) const -> bool final;
    auto Serialize(Serialized& serialized, const CredentialIndexModeFlag mode)
        const -> bool final;
    auto Sign(
        const crypto::GetPreimage input,
        crypto::SignatureRole role,
        protobuf::Signature& output,
        const PasswordPrompt& reason) const -> bool final;
    auto Sign(
        const crypto::GetPreimage input,
        crypto::SignatureRole role,
        crypto::HashType hash,
        protobuf::Signature& output,
        const PasswordPrompt& reason) const -> bool final;
    auto Sign(
        const crypto::GetPreimage input,
        crypto::SignatureRole role,
        opentxs::crypto::asymmetric::Role key,
        protobuf::Signature& output,
        const PasswordPrompt& reason) const -> bool final;
    auto Sign(
        const crypto::GetPreimage input,
        crypto::SignatureRole role,
        opentxs::crypto::asymmetric::Role key,
        crypto::HashType hash,
        protobuf::Signature& output,
        const PasswordPrompt& reason) const -> bool final;
    auto Source() const -> const identity::Source& final
    {
        return parent_.Source();
    }
    auto TransportKey(
        Data& publicKey,
        Secret& privateKey,
        const PasswordPrompt& reason) const -> bool final;
    auto Unlock(
        const crypto::asymmetric::Key& dhKey,
        const std::uint32_t tag,
        const crypto::asymmetric::Algorithm type,
        const crypto::symmetric::Key& key,
        PasswordPrompt& reason) const noexcept -> bool final;
    auto VerificationCredentialVersion() const -> VersionNumber final
    {
        return authority_to_verification_.at(version_);
    }
    auto Verify(const Data& plaintext, const protobuf::Signature& sig) const
        -> bool final;
    auto Verify(
        const Data& plaintext,
        const protobuf::Signature& sig,
        const opentxs::crypto::asymmetric::Role key) const -> bool final;
    auto Verify(const protobuf::VerificationItem& item) const -> bool final;
    auto VerifyInternally() const -> bool final;

    auto AddChildKeyCredential(
        const crypto::Parameters& parameters,
        const PasswordPrompt& reason) -> identifier::Generic final;
    auto AddVerificationCredential(
        const protobuf::VerificationSet& verificationSet,
        const PasswordPrompt& reason) -> bool final;
    auto AddContactCredential(
        const protobuf::ContactData& contactData,
        const PasswordPrompt& reason) -> bool final;
    void RevokeContactCredentials(
        UnallocatedList<identifier::Generic>& contactCredentialIDs) final;
    void RevokeVerificationCredentials(
        UnallocatedList<identifier::Generic>& verificationCredentialIDs) final;
    auto WriteCredentials() const -> bool final;

    Authority() = delete;
    Authority(const Authority&) = delete;
    Authority(Authority&&) = delete;
    auto operator=(const Authority&) -> Authority& = delete;
    auto operator=(Authority&&) -> Authority& = delete;

    ~Authority() final;

private:
    friend opentxs::Factory;
    friend internal::Authority;

    using ContactCredentialMap = UnallocatedMap<
        identifier::Generic,
        std::unique_ptr<credential::internal::Contact>>;
    using KeyCredentialMap = UnallocatedMap<
        identifier::Generic,
        std::unique_ptr<credential::internal::Secondary>>;
    using KeyCredentialItem = std::pair<
        identifier::Generic,
        std::unique_ptr<credential::internal::Secondary>>;
    using VerificationCredentialMap = UnallocatedMap<
        identifier::Generic,
        std::unique_ptr<credential::internal::Verification>>;
    using mapOfCredentials = UnallocatedMap<
        identifier::Generic,
        std::unique_ptr<credential::internal::Base>>;

    static const VersionConversionMap authority_to_contact_;
    static const VersionConversionMap authority_to_primary_;
    static const VersionConversionMap authority_to_secondary_;
    static const VersionConversionMap authority_to_verification_;
    static const VersionConversionMap nym_to_authority_;

    const api::Session& api_;
    const identity::Nym& parent_;
    const VersionNumber version_{0};
    std::uint32_t index_{0};
    std::unique_ptr<credential::internal::Primary> master_;
    KeyCredentialMap key_credentials_;
    ContactCredentialMap contact_credentials_;
    VerificationCredentialMap verification_credentials_;
    mapOfCredentials revoked_credentials_;
    protobuf::KeyMode mode_{protobuf::KEYMODE_ERROR};

    static auto is_revoked(
        const api::Session& api,
        const identifier::Generic& id,
        const String::List* plistRevokedIDs) -> bool;
    static auto create_child_credential(
        const api::Session& api,
        const crypto::Parameters& parameters,
        const identity::Source& source,
        const credential::internal::Primary& master,
        internal::Authority& parent,
        const VersionNumber parentVersion,
        crypto::Bip32Index& index,
        const opentxs::PasswordPrompt& reason) noexcept(false)
        -> KeyCredentialMap;
    static auto create_contact_credental(
        const api::Session& api,
        const crypto::Parameters& parameters,
        const identity::Source& source,
        const credential::internal::Primary& master,
        internal::Authority& parent,
        const VersionNumber parentVersion,
        const opentxs::PasswordPrompt& reason) noexcept(false)
        -> ContactCredentialMap;
    static auto create_key_credential(
        const api::Session& api,
        const crypto::Parameters& parameters,
        const identity::Source& source,
        const credential::internal::Primary& master,
        internal::Authority& parent,
        const VersionNumber parentVersion,
        crypto::Bip32Index& index,
        const opentxs::PasswordPrompt& reason) noexcept(false)
        -> KeyCredentialItem;
    static auto create_master(
        const api::Session& api,
        identity::internal::Authority& owner,
        const identity::Source& source,
        const VersionNumber version,
        const crypto::Parameters& parameters,
        const crypto::Bip32Index index,
        const opentxs::PasswordPrompt& reason) noexcept(false)
        -> std::unique_ptr<credential::internal::Primary>;
    template <typename Type>
    static void extract_child(
        const api::Session& api,
        const identity::Source& source,
        internal::Authority& authority,
        const credential::internal::Primary& master,
        const credential::internal::Base::SerializedType& serialized,
        const protobuf::KeyMode mode,
        const protobuf::CredentialRole role,
        UnallocatedMap<identifier::Generic, std::unique_ptr<Type>>&
            map) noexcept(false);
    static auto load_master(
        const api::Session& api,
        identity::internal::Authority& owner,
        const identity::Source& source,
        const protobuf::KeyMode mode,
        const Serialized& serialized) noexcept(false)
        -> std::unique_ptr<credential::internal::Primary>;
    template <typename Type>
    static auto load_child(
        const api::Session& api,
        const identity::Source& source,
        internal::Authority& authority,
        const credential::internal::Primary& master,
        const Serialized& serialized,
        const protobuf::KeyMode mode,
        const protobuf::CredentialRole role) noexcept(false)
        -> UnallocatedMap<identifier::Generic, std::unique_ptr<Type>>;

    auto get_keypair(
        const crypto::asymmetric::Algorithm type,
        const protobuf::KeyRole role,
        const String::List* plistRevokedIDs) const
        -> const crypto::key::Keypair&;
    auto get_secondary_credential(
        const identifier::Generic& strSubID,
        const String::List* plistRevokedIDs = nullptr) const
        -> const credential::Base*;

    template <typename Item>
    auto validate_credential(const Item& item) const -> bool;

    auto LoadChildKeyCredential(const String& strSubID) -> bool;
    auto LoadChildKeyCredential(const protobuf::Credential& serializedCred)
        -> bool;

    Authority(
        const api::Session& api,
        const identity::Nym& parent,
        const identity::Source& source,
        const protobuf::KeyMode mode,
        const Serialized& serialized) noexcept(false);
    Authority(
        const api::Session& api,
        const identity::Nym& parent,
        const identity::Source& source,
        const crypto::Parameters& parameters,
        VersionNumber nymVersion,
        const PasswordPrompt& reason) noexcept(false);
};
}  // namespace opentxs::identity::implementation
