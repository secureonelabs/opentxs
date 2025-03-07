// Copyright (c) 2010-2023 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include <opentxs/opentxs.hpp>
#include <chrono>
#include <ctime>

#include "internal/network/zeromq/Context.hpp"
#include "internal/network/zeromq/ListenCallback.hpp"
#include "internal/network/zeromq/socket/Pull.hpp"
#include "internal/network/zeromq/socket/Push.hpp"
#include "internal/util/Pimpl.hpp"
#include "ottest/fixtures/zeromq/PushPull.hpp"

namespace ot = opentxs;
namespace zmq = ot::network::zeromq;

namespace ottest
{
using namespace std::literals::chrono_literals;

TEST_F(PushPull, Push_Pull)
{
    bool callbackFinished{false};

    auto pullCallback = zmq::ListenCallback::Factory(
        [this, &callbackFinished](auto&& input) -> void {
            EXPECT_EQ(1, input.get().size());
            const auto inputString =
                ot::UnallocatedCString{input.Payload().begin()->Bytes()};

            EXPECT_EQ(test_message_, inputString);

            callbackFinished = true;
        });

    ASSERT_NE(nullptr, &pullCallback.get());

    auto pullSocket = context_.Internal().PullSocket(
        pullCallback, zmq::socket::Direction::Bind);

    ASSERT_NE(nullptr, &pullSocket.get());
    ASSERT_EQ(zmq::socket::Type::Pull, pullSocket->Type());

    pullSocket->SetTimeouts(0ms, 30000ms, -1ms);
    pullSocket->Start(endpoint_);

    auto pushSocket =
        context_.Internal().PushSocket(zmq::socket::Direction::Connect);

    ASSERT_NE(nullptr, &pushSocket.get());
    ASSERT_EQ(zmq::socket::Type::Push, pushSocket->Type());

    pushSocket->SetTimeouts(0ms, -1ms, 30000ms);
    pushSocket->Start(endpoint_);

    auto sent = pushSocket->Send([&] {
        auto out = opentxs::network::zeromq::Message{};
        out.AddFrame(test_message_);

        return out;
    }());

    ASSERT_TRUE(sent);

    auto end = std::time(nullptr) + 15;
    while (!callbackFinished && std::time(nullptr) < end) { ot::sleep(100ms); }

    ASSERT_TRUE(callbackFinished);
}
}  // namespace ottest
