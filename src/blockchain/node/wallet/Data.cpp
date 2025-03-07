// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "blockchain/node/wallet/Data.hpp"  // IWYU pragma: associated

#include "internal/blockchain/node/Endpoints.hpp"
#include "internal/blockchain/node/Manager.hpp"
#include "internal/network/zeromq/Context.hpp"
#include "internal/network/zeromq/socket/Raw.hpp"
#include "opentxs/api/Network.hpp"
#include "opentxs/api/network/ZeroMQ.hpp"
#include "opentxs/api/session/Client.internal.hpp"
#include "opentxs/blockchain/node/Manager.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/socket/SocketType.hpp"  // IWYU pragma: keep
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/util/Log.hpp"

namespace opentxs::blockchain::node::wallet
{
Data::Data(
    std::shared_ptr<const api::session::internal::Client> api,
    std::shared_ptr<const node::Manager> node,
    database::Wallet& db) noexcept
    : to_actor_([&] {
        using enum network::zeromq::socket::Type;
        auto out = api->Network().ZeroMQ().Context().Internal().RawSocket(Push);
        const auto rc =
            out.Connect(node->Internal().Endpoints().wallet_pull_.c_str());

        assert_true(rc);

        return out;
    }())
    , proposals_(api->asClientPublic(), *node, db, node->Internal().Chain())
{
}

Data::~Data() = default;
}  // namespace opentxs::blockchain::node::wallet
