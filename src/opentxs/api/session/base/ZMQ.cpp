// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "opentxs/api/session/base/ZMQ.hpp"  // IWYU pragma: associated

#include "opentxs/api/session/internal.factory.hpp"
#include "opentxs/util/Log.hpp"

namespace opentxs::api::session::base
{
ZMQ::ZMQ(
    const api::Crypto& crypto,
    const opentxs::network::zeromq::Context& zmq,
    const int instance) noexcept
    : zmq_context_(zmq)
    , instance_(instance)
    , endpoints_p_(factory::EndpointsAPI(crypto, instance_))
    , endpoints_(*endpoints_p_)
{
    assert_false(nullptr == endpoints_p_);
}
}  // namespace opentxs::api::session::base
