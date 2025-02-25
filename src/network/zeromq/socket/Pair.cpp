// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "network/zeromq/socket/Pair.hpp"  // IWYU pragma: associated

#include <memory>
#include <utility>

#include "internal/network/zeromq/ListenCallback.hpp"
#include "internal/network/zeromq/socket/Factory.hpp"
#include "internal/network/zeromq/socket/Pair.hpp"
#include "internal/util/Pimpl.hpp"
#include "network/zeromq/socket/Bidirectional.tpp"
#include "network/zeromq/socket/Receiver.hpp"
#include "network/zeromq/socket/Receiver.tpp"
#include "network/zeromq/socket/Sender.tpp"
#include "network/zeromq/socket/Socket.hpp"
#include "opentxs/network/zeromq/Types.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/socket/Direction.hpp"   // IWYU pragma: keep
#include "opentxs/network/zeromq/socket/SocketType.hpp"  // IWYU pragma: keep
#include "opentxs/util/Log.hpp"

template class opentxs::Pimpl<opentxs::network::zeromq::socket::Pair>;

namespace opentxs::factory
{
auto PairSocket(
    const network::zeromq::Context& context,
    const network::zeromq::ListenCallback& callback,
    const bool startThread,
    const std::string_view threadname)
    -> std::unique_ptr<network::zeromq::socket::Pair>
{
    using ReturnType = network::zeromq::socket::implementation::Pair;

    return std::make_unique<ReturnType>(
        context, callback, startThread, threadname);
}

auto PairSocket(
    const network::zeromq::ListenCallback& callback,
    const network::zeromq::socket::Pair& peer,
    const bool startThread,
    const std::string_view threadname)
    -> std::unique_ptr<network::zeromq::socket::Pair>
{
    using ReturnType = network::zeromq::socket::implementation::Pair;

    return std::make_unique<ReturnType>(
        callback, peer, startThread, threadname);
}

auto PairSocket(
    const network::zeromq::Context& context,
    const network::zeromq::ListenCallback& callback,
    const std::string_view endpoint,
    const std::string_view threadname)
    -> std::unique_ptr<network::zeromq::socket::Pair>
{
    using ReturnType = network::zeromq::socket::implementation::Pair;

    return std::make_unique<ReturnType>(
        context, callback, endpoint, threadname);
}
}  // namespace opentxs::factory

namespace opentxs::network::zeromq::socket::implementation
{
Pair::Pair(
    const zeromq::Context& context,
    const zeromq::ListenCallback& callback,
    const std::string_view endpoint,
    const Direction direction,
    const bool startThread,
    const std::string_view threadname) noexcept
    : Receiver(
          context,
          socket::Type::Pair,
          direction,
          startThread,
          CString{threadname} + " pair")
    , Bidirectional(context, true, CString{threadname} + " pair")
    , callback_(callback)
    , endpoint_(endpoint)
{
    init();
}

Pair::Pair(
    const zeromq::Context& context,
    const zeromq::ListenCallback& callback,
    const bool startThread,
    const std::string_view threadname) noexcept
    : Pair(
          context,
          callback,
          MakeArbitraryInproc(),
          Direction::Bind,
          startThread,
          threadname)
{
}

Pair::Pair(
    const zeromq::ListenCallback& callback,
    const zeromq::socket::Pair& peer,
    const bool startThread,
    const std::string_view threadname) noexcept
    : Pair(
          peer.Context(),
          callback,
          peer.Endpoint(),
          Direction::Connect,
          startThread,
          threadname)
{
}

Pair::Pair(
    const zeromq::Context& context,
    const zeromq::ListenCallback& callback,
    const std::string_view endpoint,
    const std::string_view threadname) noexcept
    : Pair(context, callback, endpoint, Direction::Connect, true, threadname)
{
}

auto Pair::clone() const noexcept -> Pair*
{
    return new Pair(context_, callback_, endpoint_, direction_, false);
}

auto Pair::Endpoint() const noexcept -> std::string_view { return endpoint_; }

auto Pair::have_callback() const noexcept -> bool { return true; }

void Pair::init() noexcept
{
    Bidirectional::init();

    assert_false(endpoint_.empty());

    const auto init = Bidirectional::Start(endpoint_);

    assert_true(init);
}

void Pair::process_incoming(const Lock& lock, Message&& message) noexcept
{
    assert_true(verify_lock(lock));

    callback_.Process(std::move(message));
}

Pair::~Pair() SHUTDOWN_SOCKET
}  // namespace opentxs::network::zeromq::socket::implementation
