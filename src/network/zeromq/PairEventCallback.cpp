// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "network/zeromq/PairEventCallback.hpp"  // IWYU pragma: associated

#include <opentxs/protobuf/PairEvent.pb.h>
#include <functional>
#include <span>

#include "internal/util/Mutex.hpp"
#include "internal/util/Pimpl.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"  // IWYU pragma: keep
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/protobuf/Types.internal.tpp"
#include "opentxs/util/Log.hpp"

template class opentxs::Pimpl<opentxs::network::zeromq::PairEventCallback>;

//"opentxs::network::zeromq::implementation::PairEventCallback::"

namespace opentxs::network::zeromq
{
auto PairEventCallback::Factory(
    zeromq::PairEventCallback::ReceiveCallback callback)
    -> OTZMQPairEventCallback
{
    return OTZMQPairEventCallback(
        new implementation::PairEventCallback(callback));
}
}  // namespace opentxs::network::zeromq

namespace opentxs::network::zeromq::implementation
{
PairEventCallback::PairEventCallback(
    zeromq::PairEventCallback::ReceiveCallback callback)
    : execute_lock_()
    , callback_lock_()
    , callback_(callback)
{
}

auto PairEventCallback::clone() const -> PairEventCallback*
{
    return new PairEventCallback(callback_);
}

auto PairEventCallback::Deactivate() const noexcept -> void
{
    static const auto null = [](const protobuf::PairEvent&) {};
    auto rlock = rLock{execute_lock_};
    auto lock = Lock{callback_lock_};
    callback_ = null;
}

auto PairEventCallback::Process(zeromq::Message&& message) const noexcept
    -> void
{
    auto body = message.Payload();

    assert_true(1 == body.size());

    const auto event = protobuf::Factory<protobuf::PairEvent>(body[0]);
    auto rlock = rLock{execute_lock_};
    auto cb = [this] {
        auto lock = Lock{callback_lock_};

        return callback_;
    }();
    cb(event);
}

PairEventCallback::~PairEventCallback() = default;
}  // namespace opentxs::network::zeromq::implementation
