// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "internal/blockchain/crypto/Factory.hpp"  // IWYU pragma: associated

#include <opentxs/protobuf/BlockchainEthereumAccountData.pb.h>
#include <opentxs/protobuf/HDPath.pb.h>
#include <memory>
#include <stdexcept>

#include "blockchain/crypto/subaccount/ethereum/Imp.hpp"
#include "internal/blockchain/crypto/Subaccount.hpp"
#include "opentxs/api/Factory.internal.hpp"
#include "opentxs/api/Session.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/identifier/HDSeed.hpp"
#include "opentxs/util/Log.hpp"

namespace opentxs::factory
{
auto BlockchainEthereumSubaccount(
    const api::Session& api,
    const blockchain::crypto::Account& parent,
    const identifier::Account& id,
    const protobuf::HDPath& path,
    const blockchain::crypto::HDProtocol standard,
    const PasswordPrompt& reason) noexcept
    -> std::shared_ptr<blockchain::crypto::internal::Subaccount>
{
    using ReturnType = blockchain::crypto::EthereumPrivate;

    try {
        auto out = std::make_shared<ReturnType>(
            api,
            parent,
            id,
            path,
            standard,
            reason,
            api.Factory().Internal().SeedID(path.seed()));
        out->InitSelf(out);

        return out;
    } catch (const std::exception& e) {
        LogVerbose()()(e.what()).Flush();

        return std::make_shared<blockchain::crypto::internal::Subaccount>();
    }
}

auto BlockchainEthereumSubaccount(
    const api::Session& api,
    const blockchain::crypto::Account& parent,
    const identifier::Account& id,
    const protobuf::BlockchainEthereumAccountData& proto) noexcept
    -> std::shared_ptr<blockchain::crypto::internal::Subaccount>
{
    using ReturnType = blockchain::crypto::EthereumPrivate;

    try {
        auto out = std::make_shared<ReturnType>(
            api,
            parent,
            id,
            proto,
            api.Factory().Internal().SeedID(proto.path().seed()));
        out->InitSelf(out);

        return out;
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return std::make_shared<blockchain::crypto::internal::Subaccount>();
    }
}
}  // namespace opentxs::factory
