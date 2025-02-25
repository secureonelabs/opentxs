// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "internal/crypto/symmetric/Factory.hpp"  // IWYU pragma: associated

#include <opentxs/protobuf/Ciphertext.pb.h>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "crypto/symmetric/KeyPrivate.hpp"
#include "internal/crypto/library/SymmetricProvider.hpp"
#include "internal/util/PMR.hpp"
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/crypto/symmetric/Algorithm.hpp"  // IWYU pragma: keep
#include "opentxs/crypto/symmetric/Types.hpp"
#include "opentxs/protobuf/syntax/SymmetricKey.hpp"  // IWYU pragma: keep
#include "opentxs/protobuf/syntax/Types.internal.tpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/Log.hpp"

namespace opentxs::factory
{
auto SymmetricKey(
    const api::Session& api,
    const crypto::SymmetricProvider& engine,
    const crypto::symmetric::Algorithm mode,
    const opentxs::PasswordPrompt& reason,
    alloc::Default alloc) noexcept -> crypto::symmetric::KeyPrivate*
{
    using ReturnType = crypto::symmetric::implementation::Key;
    using BlankType = crypto::symmetric::KeyPrivate;

    try {
        auto* out = pmr::construct<ReturnType>(alloc, api, engine);
        auto& key = *out;
        const auto realMode{
            mode == opentxs::crypto::symmetric::Algorithm::Error
                ? engine.DefaultMode()
                : mode};

        if (false == key.Derive(realMode, reason)) {

            throw std::runtime_error{"failed to derive key"};
        }

        return out;
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return pmr::default_construct<BlankType>(alloc);
    }
}

auto SymmetricKey(
    const api::Session& api,
    const crypto::SymmetricProvider& engine,
    const protobuf::SymmetricKey& serialized,
    alloc::Default alloc) noexcept -> crypto::symmetric::KeyPrivate*
{
    using ReturnType = crypto::symmetric::implementation::Key;
    using BlankType = crypto::symmetric::KeyPrivate;

    try {
        if (false == protobuf::syntax::check(LogError(), serialized)) {

            throw std::runtime_error{"invalid serialized key"};
        }

        return pmr::construct<ReturnType>(alloc, api, engine, serialized);
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return pmr::default_construct<BlankType>(alloc);
    }
}

auto SymmetricKey(
    const api::Session& api,
    const crypto::SymmetricProvider& engine,
    const opentxs::Secret& seed,
    const std::uint64_t operations,
    const std::uint64_t difficulty,
    const std::size_t size,
    const crypto::symmetric::Source type,
    alloc::Default alloc) noexcept -> crypto::symmetric::KeyPrivate*
{
    using ReturnType = crypto::symmetric::implementation::Key;
    using BlankType = crypto::symmetric::KeyPrivate;

    try {
        auto salt = ByteArray{};

        if (salt.resize(engine.SaltSize(type))) {

            throw std::runtime_error{"failed to create salt"};
        }

        return pmr::construct<ReturnType>(
            alloc,
            api,
            engine,
            seed,
            salt.Bytes(),
            size,
            operations,
            difficulty,
            0u,
            type);
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return pmr::default_construct<BlankType>(alloc);
    }
}

auto SymmetricKey(
    const api::Session& api,
    const crypto::SymmetricProvider& engine,
    const opentxs::Secret& seed,
    const ReadView salt,
    const std::uint64_t operations,
    const std::uint64_t difficulty,
    const std::uint64_t parallel,
    const std::size_t size,
    const crypto::symmetric::Source type,
    alloc::Default alloc) noexcept -> crypto::symmetric::KeyPrivate*
{
    using ReturnType = crypto::symmetric::implementation::Key;
    using BlankType = crypto::symmetric::KeyPrivate;

    try {
        return pmr::construct<ReturnType>(
            alloc,
            api,
            engine,
            seed,
            salt,
            size,
            (0u == operations) ? ReturnType::default_operations_ : operations,
            (0u == difficulty) ? ReturnType::default_difficulty_ : difficulty,
            (0u == parallel) ? ReturnType::default_threads_ : parallel,
            type);
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return pmr::default_construct<BlankType>(alloc);
    }
}

auto SymmetricKey(
    const api::Session& api,
    const crypto::SymmetricProvider& engine,
    const opentxs::Secret& raw,
    const opentxs::PasswordPrompt& reason,
    alloc::Default alloc) noexcept -> crypto::symmetric::KeyPrivate*
{
    using ReturnType = crypto::symmetric::implementation::Key;
    using BlankType = crypto::symmetric::KeyPrivate;

    try {
        auto* out = pmr::construct<ReturnType>(alloc, api, engine);
        auto& key = *out;

        if (false == key.SetRawKey(raw, reason)) {

            throw std::runtime_error{"failed to encrypt key"};
        }

        return out;
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return pmr::default_construct<BlankType>(alloc);
    }
}
}  // namespace opentxs::factory
