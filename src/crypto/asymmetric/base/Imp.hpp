// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_forward_declare opentxs::protobuf::HashType
// IWYU pragma: no_include "internal/crypto/library/AsymmetricProvider.hpp"
// IWYU pragma: no_include <Ciphertext.pb.h>
// IWYU pragma: no_include <Signature.pb.h>

#pragma once

#include <frozen/unordered_map.h>
#include <opentxs/protobuf/Enums.pb.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

#include "crypto/asymmetric/base/KeyPrivate.hpp"
#include "internal/util/Mutex.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/core/Secret.hpp"
#include "opentxs/crypto/Types.hpp"
#include "opentxs/crypto/Types.internal.hpp"
#include "opentxs/crypto/asymmetric/Key.hpp"
#include "opentxs/crypto/asymmetric/Types.hpp"
#include "opentxs/identity/Types.hpp"
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

namespace identifier
{
class Generic;
}  // namespace identifier

namespace identity
{
class Authority;
}  // namespace identity

namespace protobuf
{
class AsymmetricKey;
class Ciphertext;  // IWYU pragma: keep
class HDPath;
class Signature;
}  // namespace protobuf

class Data;
class OTSignatureMetadata;
class PasswordPrompt;
class Writer;
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::crypto::asymmetric::implementation
{
class Key : virtual public KeyPrivate
{
public:
    auto asPublic(allocator_type alloc) const noexcept -> asymmetric::Key final;
    auto CalculateHash(
        const crypto::HashType hashType,
        const PasswordPrompt& reason) const noexcept -> ByteArray final;
    auto CalculateID(identifier::Generic& theOutput) const noexcept
        -> bool final;
    auto CalculateSessionPassword(
        const asymmetric::Key& dhKey,
        const PasswordPrompt& reason,
        Secret& password) const noexcept -> bool final;
    auto CalculateTag(
        const asymmetric::Key& dhKey,
        const identifier::Generic& credential,
        const PasswordPrompt& reason,
        std::uint32_t& tag) const noexcept -> bool final;
    auto CalculateTag(
        const identity::Authority& nym,
        const Algorithm type,
        const PasswordPrompt& reason,
        std::uint32_t& tag,
        Secret& password) const noexcept -> bool final;
    auto GetMetadata() const noexcept -> const OTSignatureMetadata* final;
    auto HasCapability(identity::NymCapability capability) const noexcept
        -> bool override;
    auto HasPrivate() const noexcept -> bool final;
    auto HasPublic() const noexcept -> bool final { return has_public_; }
    auto operator==(const Serialized& rhs) const noexcept -> bool final;
    auto IsValid() const noexcept -> bool override;
    auto Path() const noexcept -> const UnallocatedCString override;
    auto Path(protobuf::HDPath& output) const noexcept -> bool override;
    auto PreferredHash() const noexcept -> crypto::HashType override;
    auto PrivateKey(const PasswordPrompt& reason) const noexcept
        -> ReadView final;
    auto Provider() const noexcept -> const crypto::AsymmetricProvider& final
    {
        return provider_;
    }
    auto PublicKey() const noexcept -> ReadView final;
    auto Role() const noexcept -> asymmetric::Role final { return role_; }
    auto Serialize(Serialized& serialized) const noexcept -> bool final;
    auto Sign(
        const GetPreimage input,
        const crypto::SignatureRole role,
        protobuf::Signature& signature,
        const identifier::Generic& credential,
        const crypto::HashType hash,
        const PasswordPrompt& reason) const noexcept -> bool final;
    auto Sign(
        const GetPreimage input,
        const crypto::SignatureRole role,
        protobuf::Signature& signature,
        const identifier::Generic& credential,
        const PasswordPrompt& reason) const noexcept -> bool final;
    auto Sign(
        ReadView preimage,
        Writer&& output,
        crypto::HashType hash,
        const PasswordPrompt& reason) const noexcept -> bool final;
    auto TransportKey(
        Data& publicKey,
        Secret& privateKey,
        const PasswordPrompt& reason) const noexcept -> bool override;
    auto Type() const noexcept -> asymmetric::Algorithm final { return type_; }
    auto Verify(const Data& plaintext, const protobuf::Signature& sig)
        const noexcept -> bool final;
    auto Verify(ReadView plaintext, ReadView sig) const noexcept -> bool final;
    auto Version() const noexcept -> VersionNumber final { return version_; }

    auto ErasePrivateData() noexcept -> bool override;

    Key() noexcept = delete;
    Key(const Key&) = delete;
    Key(Key&&) = delete;
    auto operator=(const Key& rhs) noexcept -> Key& = delete;
    auto operator=(Key&& rhs) noexcept -> Key& = delete;

    ~Key() override;

protected:
    using EncryptedKey = std::unique_ptr<protobuf::Ciphertext>;
    using EncryptedExtractor = std::function<EncryptedKey(Data&, Secret&)>;
    using PlaintextExtractor = std::function<Secret()>;

    const api::Session& api_;
    const VersionNumber version_;
    const crypto::asymmetric::Algorithm type_;
    const opentxs::crypto::asymmetric::Role role_;
    const ByteArray key_;
    mutable Secret plaintext_key_;
    mutable std::mutex lock_;
    std::unique_ptr<const protobuf::Ciphertext> encrypted_key_;

    static auto create_key(
        symmetric::Key& sessionKey,
        const crypto::AsymmetricProvider& provider,
        const Parameters& options,
        const crypto::asymmetric::Role role,
        Writer&& publicKey,
        Writer&& privateKey,
        const opentxs::Secret& prv,
        Writer&& params,
        const PasswordPrompt& reason) noexcept(false)
        -> std::unique_ptr<protobuf::Ciphertext>;
    static auto encrypt_key(
        ReadView plaintext,
        bool attach,
        symmetric::Key& sessionKey,
        const PasswordPrompt& reason) noexcept
        -> std::unique_ptr<protobuf::Ciphertext>;
    static auto encrypt_key(
        ReadView plaintext,
        bool attach,
        symmetric::Key& sessionKey,
        const PasswordPrompt& reason,
        protobuf::Ciphertext& out) noexcept -> bool;
    static auto generate_key(
        const crypto::AsymmetricProvider& provider,
        const Parameters& options,
        const crypto::asymmetric::Role role,
        Writer&& publicKey,
        Writer&& privateKey,
        Writer&& params) noexcept(false) -> void;

    auto get_private_key(const Lock& lock, const PasswordPrompt& reason) const
        noexcept(false) -> Secret&;
    auto has_private(const Lock& lock) const noexcept -> bool;
    auto private_key(const Lock& lock, const PasswordPrompt& reason)
        const noexcept -> ReadView;
    virtual auto serialize(const Lock& lock, Serialized& serialized)
        const noexcept -> bool;

    virtual auto erase_private_data(const Lock& lock) -> void;

    Key(const api::Session& api,
        const crypto::AsymmetricProvider& engine,
        const crypto::asymmetric::Algorithm keyType,
        const crypto::asymmetric::Role role,
        const bool hasPublic,
        const bool hasPrivate,
        const VersionNumber version,
        ByteArray&& pubkey,
        EncryptedExtractor get,
        PlaintextExtractor getPlaintext,
        allocator_type alloc) noexcept(false);
    Key(const api::Session& api,
        const crypto::AsymmetricProvider& engine,
        const crypto::asymmetric::Algorithm keyType,
        const crypto::asymmetric::Role role,
        const VersionNumber version,
        EncryptedExtractor get,
        allocator_type alloc) noexcept(false);
    Key(const api::Session& api,
        const crypto::AsymmetricProvider& engine,
        const protobuf::AsymmetricKey& serializedKey,
        EncryptedExtractor get,
        allocator_type alloc) noexcept(false);
    Key(const Key& rhs, allocator_type alloc) noexcept;
    Key(const Key& rhs,
        const ReadView newPublic,
        allocator_type alloc) noexcept;
    Key(const Key& rhs,
        ByteArray&& newPublicKey,
        Secret&& newSecretKey,
        allocator_type alloc) noexcept;

private:
    static constexpr auto HashTypeMapSize = std::size_t{16};
    using HashTypeMap = frozen::
        unordered_map<crypto::HashType, protobuf::HashType, HashTypeMapSize>;
    using HashTypeReverseMap = frozen::
        unordered_map<protobuf::HashType, crypto::HashType, HashTypeMapSize>;
    static constexpr auto SignatureRoleMapSize = std::size_t{12};
    using SignatureRoleMap = frozen::unordered_map<
        crypto::SignatureRole,
        protobuf::SignatureRole,
        SignatureRoleMapSize>;

    static const frozen::unordered_map<
        crypto::SignatureRole,
        VersionNumber,
        SignatureRoleMapSize>
        sig_version_;

    const crypto::AsymmetricProvider& provider_;
    const bool has_public_;
    const std::unique_ptr<const OTSignatureMetadata> metadata_;
    bool has_private_;

    auto SerializeKeyToData(const protobuf::AsymmetricKey& rhs) const
        -> ByteArray;

    static auto hashtype_map() noexcept -> const HashTypeMap&;
    static auto signaturerole_map() noexcept -> const SignatureRoleMap&;
    static auto translate(const crypto::SignatureRole in) noexcept
        -> protobuf::SignatureRole;
    static auto translate(const crypto::HashType in) noexcept
        -> protobuf::HashType;
    static auto translate(const protobuf::HashType in) noexcept
        -> crypto::HashType;

    auto get_password(
        const Lock& lock,
        const asymmetric::Key& target,
        const PasswordPrompt& reason,
        Secret& password) const noexcept -> bool;
    auto get_tag(
        const Lock& lock,
        const asymmetric::Key& target,
        const identifier::Generic& credential,
        const PasswordPrompt& reason,
        std::uint32_t& tag) const noexcept -> bool;
    auto new_signature(
        const identifier::Generic& credentialID,
        const crypto::SignatureRole role,
        const crypto::HashType hash) const -> protobuf::Signature;
};
}  // namespace opentxs::crypto::asymmetric::implementation
