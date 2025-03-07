// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "opentxs/rpc/response/MessagePrivate.hpp"  // IWYU pragma: associated
#include "opentxs/rpc/response/SendPayment.hpp"     // IWYU pragma: associated

#include <memory>
#include <utility>

#include "opentxs/rpc/request/SendPayment.hpp"
#include "opentxs/util/Writer.hpp"

namespace opentxs::rpc::response::implementation
{
struct SendPayment final : public Message::Imp {
    auto asSendPayment() const noexcept -> const response::SendPayment& final
    {
        return static_cast<const response::SendPayment&>(*parent_);
    }
    auto serialize(protobuf::RPCResponse& dest) const noexcept -> bool final
    {
        if (Imp::serialize(dest)) {
            serialize_tasks(dest);

            return true;
        }

        return false;
    }

    SendPayment(
        const response::SendPayment* parent,
        const request::SendPayment& request,
        Message::Responses&& response,
        Message::Tasks&& tasks) noexcept(false)
        : Imp(parent, request, std::move(response), std::move(tasks))
    {
    }
    SendPayment(
        const response::SendPayment* parent,
        const protobuf::RPCResponse& in) noexcept(false)
        : Imp(parent, in)
    {
    }
    SendPayment() = delete;
    SendPayment(const SendPayment&) = delete;
    SendPayment(SendPayment&&) = delete;
    auto operator=(const SendPayment&) -> SendPayment& = delete;
    auto operator=(SendPayment&&) -> SendPayment& = delete;

    ~SendPayment() final = default;
};
}  // namespace opentxs::rpc::response::implementation

namespace opentxs::rpc::response
{
SendPayment::SendPayment(
    const request::SendPayment& request,
    Responses&& response,
    Tasks&& tasks)
    : Message(std::make_unique<implementation::SendPayment>(
          this,
          request,
          std::move(response),
          std::move(tasks)))
{
}

SendPayment::SendPayment(const protobuf::RPCResponse& serialized) noexcept(
    false)
    : Message(std::make_unique<implementation::SendPayment>(this, serialized))
{
}

SendPayment::SendPayment() noexcept
    : Message()
{
}

auto SendPayment::Pending() const noexcept -> const Tasks&
{
    return imp_->tasks_;
}

SendPayment::~SendPayment() = default;
}  // namespace opentxs::rpc::response
