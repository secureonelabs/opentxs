// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <chrono>
#include <source_location>

#include "opentxs/Types.hpp"
#include "opentxs/network/zeromq/Types.internal.hpp"
#include "opentxs/network/zeromq/socket/Types.internal.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs
{
namespace network
{
namespace zeromq
{
namespace socket
{
class Raw;
}  // namespace socket

class Message;
}  // namespace zeromq
}  // namespace network
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::network::zeromq::socket
{
auto swap(Raw& lhs, Raw& rhs) noexcept -> void;

class Raw
{
public:
    class Imp;

    auto ID() const noexcept -> SocketID;
    auto Type() const noexcept -> socket::Type;

    auto Bind(const char* endpoint) noexcept -> bool;
    auto ClearSubscriptions() noexcept -> bool;
    auto Close() noexcept -> void;
    auto Connect(const char* endpoint) noexcept -> bool;
    auto Disconnect(const char* endpoint) noexcept -> bool;
    auto DisconnectAll() noexcept -> bool;
    auto EnableCurveClient(
        const ReadView serverKey,
        const ReadView publicKey,
        const ReadView secretKey) noexcept -> bool;
    auto EnableCurveServer(const ReadView secretKey) noexcept -> bool;
    auto Native() noexcept -> void*;
    /** Send to a recipient in the same process
     *
     *  This function aborts if the message can not be sent.
     */
    auto Send(
        Message&& msg,
        bool silent = true,
        const std::source_location& loc =
            std::source_location::current()) noexcept -> bool;
    /** Send to a recipient without ZMQ_DONTWAIT
     */
    auto SendDeferred(
        Message&& msg,
        bool silent = true,
        const std::source_location& loc =
            std::source_location::current()) noexcept -> bool;
    /** Send to a remote recipient
     *
     *  This function returns false if the message can not be sent.
     */
    auto SendExternal(
        Message&& msg,
        bool silent = true,
        const std::source_location& loc =
            std::source_location::current()) noexcept -> bool;
    auto SetExposedUntrusted() noexcept -> bool;
    auto SetIncomingHWM(int value) noexcept -> bool;
    auto SetLinger(int value) noexcept -> bool;
    auto SetMaxMessageSize(std::size_t bytes) noexcept -> bool;
    auto SetMonitor(const char* endpoint, int events) noexcept -> bool;
    auto SetOutgoingHWM(int value) noexcept -> bool;
    auto SetPrivateKey(ReadView key) noexcept -> bool;
    auto SetRouterHandover(bool value) noexcept -> bool;
    auto SetRoutingID(ReadView id) noexcept -> bool;
    auto SetSendTimeout(std::chrono::milliseconds value) noexcept -> bool;
    auto SetZAPDomain(ReadView domain) noexcept -> bool;
    auto Stop() noexcept -> void;
    auto swap(Raw& other) noexcept -> void;
    auto Unbind(const char* endpoint) noexcept -> bool;
    auto UnbindAll() noexcept -> bool;
    auto WaitForSend() noexcept -> bool;

    Raw(Imp* imp) noexcept;
    Raw() = delete;
    Raw(const Raw&) = delete;
    Raw(Raw&&) noexcept;
    auto operator=(const Raw&) -> Raw& = delete;
    auto operator=(Raw&&) noexcept -> Raw&;

    ~Raw();

private:
    Imp* imp_;
};
}  // namespace opentxs::network::zeromq::socket
