// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "internal/network/zeromq/curve/Server.hpp"
#include "internal/network/zeromq/socket/Sender.hpp"
#include "internal/util/Pimpl.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs
{
namespace network
{
namespace zeromq
{
namespace socket
{
class Publish;
}  // namespace socket
}  // namespace zeromq
}  // namespace network

using OTZMQPublishSocket = Pimpl<network::zeromq::socket::Publish>;
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::network::zeromq::socket
{
class Publish : virtual public curve::Server, virtual public Sender
{
public:
    Publish(const Publish&) = delete;
    Publish(Publish&&) = delete;
    auto operator=(const Publish&) -> Publish& = delete;
    auto operator=(Publish&&) -> Publish& = delete;

    ~Publish() override = default;

protected:
    Publish() noexcept = default;

private:
    friend OTZMQPublishSocket;

    virtual auto clone() const noexcept -> Publish* = 0;
};
}  // namespace opentxs::network::zeromq::socket
