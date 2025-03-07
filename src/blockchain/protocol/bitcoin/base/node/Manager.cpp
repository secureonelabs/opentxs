// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_forward_declare opentxs::blockchain::node::Manager

#include "blockchain/protocol/bitcoin/base/node/Manager.hpp"  // IWYU pragma: associated

#include <opentxs/protobuf/BlockchainBlockHeader.pb.h>  // IWYU pragma: keep
#include <memory>

#include "internal/blockchain/node/Factory.hpp"
#include "opentxs/blockchain/node/Manager.hpp"

namespace opentxs::factory
{
auto BlockchainNetworkBitcoin(
    const api::session::Client& api,
    const blockchain::Type type,
    const blockchain::node::internal::Config& config,
    std::string_view seednode) noexcept
    -> std::shared_ptr<blockchain::node::Manager>
{
    using ReturnType = blockchain::node::base::Bitcoin;

    return std::make_shared<ReturnType>(api, type, config, seednode);
}
}  // namespace opentxs::factory

namespace opentxs::blockchain::node::base
{
Bitcoin::Bitcoin(
    const api::session::Client& api,
    const Type type,
    const internal::Config& config,
    std::string_view seednode)
    : ot_super(api, type, config, seednode)
{
}

Bitcoin::~Bitcoin() { Shutdown(); }
}  // namespace opentxs::blockchain::node::base
