// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "crypto/asymmetric/key/rsa/Imp.hpp"  // IWYU pragma: associated

#include <opentxs/protobuf/AsymmetricKey.pb.h>
#include <opentxs/protobuf/Ciphertext.pb.h>
#include <stdexcept>
#include <string>

#include "crypto/asymmetric/base/KeyPrivate.hpp"
#include "opentxs/api/Session.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/crypto/HashType.hpp"  // IWYU pragma: keep
#include "opentxs/crypto/Types.hpp"
#include "opentxs/crypto/asymmetric/Algorithm.hpp"  // IWYU pragma: keep
#include "opentxs/crypto/asymmetric/Role.hpp"       // IWYU pragma: keep
#include "opentxs/crypto/asymmetric/Types.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Writer.hpp"

namespace opentxs::crypto::asymmetric::key::implementation
{
RSA::RSA(
    const api::Session& api,
    const crypto::AsymmetricProvider& engine,
    const protobuf::AsymmetricKey& serialized,
    allocator_type alloc) noexcept(false)
    : KeyPrivate(alloc)
    , RSAPrivate(alloc)
    , Key(
          api,
          engine,
          serialized,
          [&](auto& pub, auto& prv) -> EncryptedKey {
              return deserialize_key(api, serialized, pub, prv);
          },
          alloc)
    , params_(api_.Factory().DataFromBytes(serialized.params()))
    , self_(this)
{
}

RSA::RSA(
    const api::Session& api,
    const crypto::AsymmetricProvider& engine,
    const crypto::asymmetric::Role role,
    const VersionNumber version,
    const Parameters& options,
    Space& params,
    symmetric::Key& sessionKey,
    const PasswordPrompt& reason,
    allocator_type alloc) noexcept(false)
    : KeyPrivate(alloc)
    , RSAPrivate(alloc)
    , Key(
          api,
          engine,
          crypto::asymmetric::Algorithm::Legacy,
          role,
          version,
          [&](auto& pub, auto& prv) -> EncryptedKey {
              return create_key(
                  sessionKey,
                  engine,
                  options,
                  role,
                  pub.WriteInto(),
                  prv.WriteInto(),
                  prv,
                  writer(params),
                  reason);
          },
          alloc)
    , params_(api_.Factory().Data(params))
    , self_(this)
{
    if (false == bool(encrypted_key_)) {
        throw std::runtime_error("Failed to instantiate encrypted_key_");
    }
}

RSA::RSA(const RSA& rhs, allocator_type alloc) noexcept
    : KeyPrivate(alloc)
    , RSAPrivate(alloc)
    , Key(rhs, alloc)
    , params_(rhs.params_)
    , self_(this)
{
}

auto RSA::deserialize_key(
    const api::Session& api,
    const protobuf::AsymmetricKey& proto,
    Data& publicKey,
    Secret&) noexcept(false) -> std::unique_ptr<protobuf::Ciphertext>
{
    auto output = std::unique_ptr<protobuf::Ciphertext>{};
    publicKey.Assign(proto.key());

    if (proto.has_encryptedkey()) {
        output = std::make_unique<protobuf::Ciphertext>(proto.encryptedkey());

        assert_false(nullptr == output);
    }

    return output;
}

auto RSA::PreferredHash() const noexcept -> crypto::HashType
{
    return crypto::HashType::Sha256;
}

auto RSA::serialize(const Lock& lock, Serialized& output) const noexcept -> bool
{
    if (false == Key::serialize(lock, output)) { return false; }

    if (crypto::asymmetric::Role::Encrypt == role_) {
        output.set_params(params_.data(), params_.size());
    }

    return true;
}

RSA::~RSA() { Reset(self_); }
}  // namespace opentxs::crypto::asymmetric::key::implementation
