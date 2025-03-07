// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "api/session/Workflow.hpp"  // IWYU pragma: associated

#include <opentxs/protobuf/AccountEvent.pb.h>
#include <opentxs/protobuf/InstrumentRevision.pb.h>
#include <opentxs/protobuf/PaymentEvent.pb.h>
#include <opentxs/protobuf/PaymentWorkflow.pb.h>
#include <opentxs/protobuf/PaymentWorkflowEnums.pb.h>
#include <opentxs/protobuf/Purse.pb.h>
#include <opentxs/protobuf/RPCEnums.pb.h>
#include <opentxs/protobuf/RPCPush.pb.h>
#include <algorithm>
#include <chrono>
#include <compare>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>

#include "internal/api/session/Storage.hpp"
#include "internal/api/session/Types.hpp"
#include "internal/core/String.hpp"
#include "internal/network/zeromq/Context.hpp"
#include "internal/network/zeromq/message/Message.hpp"
#include "internal/network/zeromq/socket/Publish.hpp"
#include "internal/network/zeromq/socket/Push.hpp"
#include "internal/otx/blind/Purse.hpp"
#include "internal/otx/common/Cheque.hpp"
#include "internal/otx/common/Message.hpp"
#include "internal/otx/common/OTTransaction.hpp"
#include "internal/util/Pimpl.hpp"
#include "opentxs/Time.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/api/Factory.internal.hpp"
#include "opentxs/api/Network.hpp"
#include "opentxs/api/Session.hpp"
#include "opentxs/api/network/ZeroMQ.hpp"
#include "opentxs/api/session/Activity.hpp"
#include "opentxs/api/session/Contacts.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Factory.internal.hpp"
#include "opentxs/api/session/Storage.hpp"
#include "opentxs/api/session/Workflow.hpp"
#include "opentxs/api/session/internal.factory.hpp"
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/identifier/Account.hpp"
#include "opentxs/identifier/Generic.hpp"
#include "opentxs/identifier/Notary.hpp"
#include "opentxs/identifier/Nym.hpp"
#include "opentxs/identifier/UnitDefinition.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/Types.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/message/Message.tpp"
#include "opentxs/network/zeromq/socket/Direction.hpp"  // IWYU pragma: keep
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/otx/Types.internal.hpp"
#include "opentxs/otx/blind/Purse.hpp"
#include "opentxs/otx/client/PaymentWorkflowState.hpp"  // IWYU pragma: keep
#include "opentxs/otx/client/PaymentWorkflowType.hpp"   // IWYU pragma: keep
#include "opentxs/otx/client/StorageBox.hpp"            // IWYU pragma: keep
#include "opentxs/otx/client/Types.hpp"
#include "opentxs/protobuf/Types.internal.hpp"
#include "opentxs/protobuf/Types.internal.tpp"
#include "opentxs/protobuf/syntax/PaymentWorkflow.hpp"
#include "opentxs/protobuf/syntax/RPCPush.hpp"
#include "opentxs/protobuf/syntax/Types.internal.tpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Writer.hpp"

namespace opentxs
{
constexpr auto RPC_ACCOUNT_EVENT_VERSION = 1;
constexpr auto RPC_PUSH_VERSION = 1;
}  // namespace opentxs

namespace zmq = opentxs::network::zeromq;

namespace opentxs::factory
{
auto Workflow(
    const api::Session& api,
    const api::session::Activity& activity,
    const api::session::Contacts& contact) noexcept
    -> std::unique_ptr<api::session::Workflow>
{
    using ReturnType = api::session::imp::Workflow;

    return std::make_unique<ReturnType>(api, activity, contact);
}
}  // namespace opentxs::factory

namespace opentxs::api::session
{
using PaymentWorkflowState = otx::client::PaymentWorkflowState;
using PaymentWorkflowType = otx::client::PaymentWorkflowType;

auto Workflow::ContainsCash(const protobuf::PaymentWorkflow& workflow) -> bool
{
    switch (translate(workflow.type())) {
        case PaymentWorkflowType::OutgoingCash:
        case PaymentWorkflowType::IncomingCash: {
            return true;
        }
        case PaymentWorkflowType::Error:
        case PaymentWorkflowType::OutgoingCheque:
        case PaymentWorkflowType::IncomingCheque:
        case PaymentWorkflowType::OutgoingInvoice:
        case PaymentWorkflowType::IncomingInvoice:
        case PaymentWorkflowType::OutgoingTransfer:
        case PaymentWorkflowType::IncomingTransfer:
        case PaymentWorkflowType::InternalTransfer:
        default: {
        }
    }

    return false;
}

auto Workflow::ContainsCheque(const protobuf::PaymentWorkflow& workflow) -> bool
{
    switch (translate(workflow.type())) {
        case PaymentWorkflowType::OutgoingCheque:
        case PaymentWorkflowType::IncomingCheque:
        case PaymentWorkflowType::OutgoingInvoice:
        case PaymentWorkflowType::IncomingInvoice: {
            return true;
        }
        case PaymentWorkflowType::Error:
        case PaymentWorkflowType::OutgoingTransfer:
        case PaymentWorkflowType::IncomingTransfer:
        case PaymentWorkflowType::InternalTransfer:
        case PaymentWorkflowType::OutgoingCash:
        case PaymentWorkflowType::IncomingCash:
        default: {
        }
    }

    return false;
}

auto Workflow::ContainsTransfer(const protobuf::PaymentWorkflow& workflow)
    -> bool
{
    switch (translate(workflow.type())) {
        case PaymentWorkflowType::OutgoingTransfer:
        case PaymentWorkflowType::IncomingTransfer:
        case PaymentWorkflowType::InternalTransfer: {
            return true;
        }
        case PaymentWorkflowType::Error:
        case PaymentWorkflowType::OutgoingCheque:
        case PaymentWorkflowType::IncomingCheque:
        case PaymentWorkflowType::OutgoingInvoice:
        case PaymentWorkflowType::IncomingInvoice:
        case PaymentWorkflowType::OutgoingCash:
        case PaymentWorkflowType::IncomingCash:
        default: {
        }
    }

    return false;
}

auto Workflow::ExtractCheque(const protobuf::PaymentWorkflow& workflow)
    -> UnallocatedCString
{
    if (false == ContainsCheque(workflow)) {
        LogError()()("Wrong workflow type").Flush();

        return {};
    }

    if (1 != workflow.source().size()) {
        LogError()()("Invalid workflow").Flush();

        return {};
    }

    return workflow.source(0).item();
}

auto Workflow::ExtractPurse(
    const protobuf::PaymentWorkflow& workflow,
    protobuf::Purse& out) -> bool
{
    if (false == ContainsCash(workflow)) {
        LogError()()("Wrong workflow type").Flush();

        return false;
    }

    if (1 != workflow.source().size()) {
        LogError()()("Invalid workflow").Flush();

        return false;
    }

    const auto& serialized = workflow.source(0).item();
    out = protobuf::Factory<protobuf::Purse>(serialized);

    return true;
}

auto Workflow::ExtractTransfer(const protobuf::PaymentWorkflow& workflow)
    -> UnallocatedCString
{
    if (false == ContainsTransfer(workflow)) {
        LogError()()("Wrong workflow type").Flush();

        return {};
    }

    if (1 != workflow.source().size()) {
        LogError()()("Invalid workflow").Flush();

        return {};
    }

    return workflow.source(0).item();
}

auto Workflow::InstantiateCheque(
    const api::Session& api,
    const protobuf::PaymentWorkflow& workflow) -> Workflow::Cheque
{
    Cheque output{PaymentWorkflowState::Error, nullptr};
    auto& [state, cheque] = output;

    switch (translate(workflow.type())) {
        case PaymentWorkflowType::OutgoingCheque:
        case PaymentWorkflowType::IncomingCheque:
        case PaymentWorkflowType::OutgoingInvoice:
        case PaymentWorkflowType::IncomingInvoice: {
            cheque = api.Factory().Internal().Session().Cheque();

            assert_false(nullptr == cheque);

            const auto serialized = ExtractCheque(workflow);

            if (serialized.empty()) { return output; }

            const auto loaded = cheque->LoadContractFromString(
                String::Factory(serialized.c_str()));

            if (false == loaded) {
                LogError()()("Failed to instantiate cheque").Flush();
                cheque.reset();

                return output;
            }

            state = translate(workflow.state());
        } break;
        case PaymentWorkflowType::Error:
        case PaymentWorkflowType::OutgoingTransfer:
        case PaymentWorkflowType::IncomingTransfer:
        case PaymentWorkflowType::InternalTransfer:
        case PaymentWorkflowType::OutgoingCash:
        case PaymentWorkflowType::IncomingCash:
        default: {
            LogError()()("Incorrect workflow type").Flush();
        }
    }

    return output;
}

auto Workflow::InstantiatePurse(
    const api::Session& api,
    const protobuf::PaymentWorkflow& workflow) -> Workflow::Purse
{
    auto output = Purse{};
    auto& [state, purse] = output;
    state = PaymentWorkflowState::Error;

    switch (translate(workflow.type())) {
        case PaymentWorkflowType::OutgoingCash:
        case PaymentWorkflowType::IncomingCash: {
            try {
                const auto serialized = [&] {
                    auto out = protobuf::Purse{};

                    if (false == ExtractPurse(workflow, out)) {
                        throw std::runtime_error{"Missing purse"};
                    }

                    return out;
                }();

                purse = api.Factory().Internal().Session().Purse(serialized);

                if (false == bool(purse)) {
                    throw std::runtime_error{"Failed to instantiate purse"};
                }

                state = translate(workflow.state());
            } catch (const std::exception& e) {
                LogError()()(e.what()).Flush();

                return output;
            }
        } break;
        case PaymentWorkflowType::Error:
        case PaymentWorkflowType::OutgoingCheque:
        case PaymentWorkflowType::IncomingCheque:
        case PaymentWorkflowType::OutgoingInvoice:
        case PaymentWorkflowType::IncomingInvoice:
        case PaymentWorkflowType::OutgoingTransfer:
        case PaymentWorkflowType::IncomingTransfer:
        case PaymentWorkflowType::InternalTransfer:
        default: {
            LogError()()("Incorrect workflow type").Flush();
        }
    }

    return output;
}

auto Workflow::InstantiateTransfer(
    const api::Session& api,
    const protobuf::PaymentWorkflow& workflow) -> Workflow::Transfer
{
    Transfer output{PaymentWorkflowState::Error, nullptr};
    auto& [state, transfer] = output;

    switch (translate(workflow.type())) {
        case PaymentWorkflowType::OutgoingTransfer:
        case PaymentWorkflowType::IncomingTransfer:
        case PaymentWorkflowType::InternalTransfer: {
            const auto serialized = ExtractTransfer(workflow);

            if (serialized.empty()) { return output; }

            transfer = api.Factory().Internal().Session().Item(serialized);

            if (false == bool(transfer)) {
                LogError()()("Failed to instantiate transfer").Flush();
                transfer.reset();

                return output;
            }

            state = translate(workflow.state());
        } break;

        case PaymentWorkflowType::Error:
        case PaymentWorkflowType::OutgoingCheque:
        case PaymentWorkflowType::IncomingCheque:
        case PaymentWorkflowType::OutgoingInvoice:
        case PaymentWorkflowType::IncomingInvoice:
        case PaymentWorkflowType::OutgoingCash:
        case PaymentWorkflowType::IncomingCash:
        default: {
            LogError()()("Incorrect workflow type").Flush();
        }
    }

    return output;
}

auto Workflow::UUID(
    const api::Session& api,
    const protobuf::PaymentWorkflow& workflow) -> identifier::Generic
{
    auto output = identifier::Generic{};
    auto notaryID = identifier::Generic{};
    TransactionNumber number{0};

    switch (translate(workflow.type())) {
        case PaymentWorkflowType::OutgoingCheque:
        case PaymentWorkflowType::IncomingCheque:
        case PaymentWorkflowType::OutgoingInvoice:
        case PaymentWorkflowType::IncomingInvoice: {
            [[maybe_unused]] auto [state, cheque] =
                InstantiateCheque(api, workflow);

            if (false == bool(cheque)) {
                LogError()()("Invalid cheque").Flush();

                return output;
            }

            notaryID = cheque->GetNotaryID();
            number = cheque->GetTransactionNum();
        } break;
        case PaymentWorkflowType::OutgoingTransfer:
        case PaymentWorkflowType::IncomingTransfer:
        case PaymentWorkflowType::InternalTransfer: {
            [[maybe_unused]] auto [state, transfer] =
                InstantiateTransfer(api, workflow);

            if (false == bool(transfer)) {
                LogError()()("Invalid transfer").Flush();

                return output;
            }

            notaryID = transfer->GetPurportedNotaryID();
            number = transfer->GetTransactionNum();
        } break;
        case PaymentWorkflowType::OutgoingCash:
        case PaymentWorkflowType::IncomingCash: {
            // TODO
        } break;
        case PaymentWorkflowType::Error:
        default: {
            LogError()()("Unknown workflow type").Flush();
        }
    }

    return UUID(api, notaryID, number);
}

auto Workflow::UUID(
    const api::Session& api,
    const identifier::Generic& notary,
    const TransactionNumber& number) -> identifier::Generic
{
    LogTrace()()("UUID for notary ")(notary, api.Crypto())(
        " and transaction number ")(number)(" is ");
    auto preimage = api.Factory().Data();
    preimage.Assign(notary);
    preimage.Concatenate(&number, sizeof(number));

    return api.Factory().IdentifierFromPreimage(preimage.Bytes());
}
}  // namespace opentxs::api::session

namespace opentxs::api::session::imp
{
using PaymentWorkflowState = otx::client::PaymentWorkflowState;
using PaymentWorkflowType = otx::client::PaymentWorkflowType;

const Workflow::VersionMap Workflow::versions_{
    {PaymentWorkflowType::OutgoingCheque, {1, 1, 1}},
    {PaymentWorkflowType::IncomingCheque, {1, 1, 1}},
    {PaymentWorkflowType::OutgoingTransfer, {2, 1, 2}},
    {PaymentWorkflowType::IncomingTransfer, {2, 1, 2}},
    {PaymentWorkflowType::InternalTransfer, {2, 1, 2}},
    {PaymentWorkflowType::OutgoingCash, {3, 1, 3}},
    {PaymentWorkflowType::IncomingCash, {3, 1, 3}},
};

Workflow::Workflow(
    const api::Session& api,
    const api::session::Activity& activity,
    const api::session::Contacts& contact)
    : api_(api)
    , activity_(activity)
    , contact_(contact)
    , account_publisher_(
          api_.Network().ZeroMQ().Context().Internal().PublishSocket())
    , rpc_publisher_(api_.Network().ZeroMQ().Context().Internal().PushSocket(
          zmq::socket::Direction::Connect))
    , workflow_locks_()
{
    // WARNING: do not access api_.Wallet() during construction
    const auto endpoint = api_.Endpoints().WorkflowAccountUpdate();
    LogDetail()()("Binding to ")(endpoint.data()).Flush();
    auto bound = account_publisher_->Start(endpoint.data());

    assert_true(bound);

    bound =
        rpc_publisher_->Start(opentxs::network::zeromq::MakeDeterministicInproc(
            "rpc/push/internal", -1, 1));

    assert_true(bound);
}

auto Workflow::AbortTransfer(
    const identifier::Nym& nymID,
    const Item& transfer,
    const Message& reply) const -> bool
{
    if (false == isTransfer(transfer)) { return false; }

    const bool isInternal = isInternalTransfer(
        transfer.GetRealAccountID(), transfer.GetDestinationAcctID());
    const UnallocatedSet<PaymentWorkflowType> type{
        isInternal ? PaymentWorkflowType::InternalTransfer
                   : PaymentWorkflowType::OutgoingTransfer};
    auto global = Lock{lock_};
    const auto workflow = get_workflow(global, type, nymID, transfer);

    if (false == bool(workflow)) {
        LogError()()("Workflow for this transfer does not exist.").Flush();

        return false;
    }

    auto lock = get_workflow_lock(global, workflow->id());

    if (false == can_abort_transfer(*workflow)) { return false; }

    return add_transfer_event(
        lock,
        nymID,
        {},
        *workflow,
        PaymentWorkflowState::Aborted,
        protobuf::PAYMENTEVENTTYPE_ABORT,
        (isInternal
             ? versions_.at(PaymentWorkflowType::InternalTransfer).event_
             : versions_.at(PaymentWorkflowType::OutgoingTransfer).event_),
        reply,
        transfer.GetRealAccountID(),
        true);
}

// Works for Incoming and Internal transfer workflows.
auto Workflow::AcceptTransfer(
    const identifier::Nym& nymID,
    const identifier::Notary& notaryID,
    const OTTransaction& pending,
    const Message& reply) const -> bool
{
    const auto transfer = extract_transfer_from_pending(pending);

    if (false == bool(transfer)) {
        LogError()()("Invalid transaction").Flush();

        return false;
    }

    const auto& senderNymID = transfer->GetNymID();
    const auto& recipientNymID = pending.GetNymID();
    const auto& accountID = pending.GetPurportedAccountID();

    if (pending.GetNymID() != nymID) {
        LogError()()("Invalid recipient").Flush();

        return false;
    }

    const bool isInternal = (senderNymID == recipientNymID);

    // Ignore this event for internal transfers.
    if (isInternal) { return true; }

    const UnallocatedSet<PaymentWorkflowType> type{
        PaymentWorkflowType::IncomingTransfer};
    auto global = Lock{lock_};
    const auto workflow = get_workflow(global, type, nymID, *transfer);

    if (false == bool(workflow)) {
        LogError()()("Workflow for this transfer does not exist.").Flush();

        return false;
    }

    auto lock = get_workflow_lock(global, workflow->id());

    if (false == can_accept_transfer(*workflow)) { return false; }

    return add_transfer_event(
        lock,
        nymID,
        senderNymID,
        *workflow,
        PaymentWorkflowState::Completed,
        protobuf::PAYMENTEVENTTYPE_ACCEPT,
        versions_.at(PaymentWorkflowType::OutgoingTransfer).event_,
        reply,
        accountID,
        true);
}

auto Workflow::AcknowledgeTransfer(
    const identifier::Nym& nymID,
    const Item& transfer,
    const Message& reply) const -> bool
{
    if (false == isTransfer(transfer)) { return false; }

    const bool isInternal = isInternalTransfer(
        transfer.GetRealAccountID(), transfer.GetDestinationAcctID());
    const UnallocatedSet<PaymentWorkflowType> type{
        isInternal ? PaymentWorkflowType::InternalTransfer
                   : PaymentWorkflowType::OutgoingTransfer};
    auto global = Lock{lock_};
    const auto workflow = get_workflow(global, type, nymID, transfer);

    if (false == bool(workflow)) {
        LogError()()("Workflow for this transfer does not exist.").Flush();

        return false;
    }

    auto lock = get_workflow_lock(global, workflow->id());

    if (false == can_acknowledge_transfer(*workflow)) { return false; }

    // For internal transfers it's possible that a push notification already
    // advanced the state to conveyed before the sender received the
    // acknowledgement. The timing of those two events is indeterminate,
    // therefore if the state has already advanced, add the acknowledge event
    // but do not change the state.
    const PaymentWorkflowState state =
        (PaymentWorkflowState::Conveyed == translate(workflow->state()))
            ? PaymentWorkflowState::Conveyed
            : PaymentWorkflowState::Acknowledged;

    return add_transfer_event(
        lock,
        nymID,
        {},
        *workflow,
        state,
        protobuf::PAYMENTEVENTTYPE_ACKNOWLEDGE,
        (isInternal
             ? versions_.at(PaymentWorkflowType::InternalTransfer).event_
             : versions_.at(PaymentWorkflowType::OutgoingTransfer).event_),
        reply,
        transfer.GetRealAccountID(),
        true);
}

auto Workflow::AllocateCash(
    const identifier::Nym& id,
    const otx::blind::Purse& purse) const -> identifier::Generic
{
    const auto global = Lock{lock_};
    auto workflowID = api_.Factory().IdentifierFromRandom();
    protobuf::PaymentWorkflow workflow{};
    workflow.set_version(
        versions_.at(otx::client::PaymentWorkflowType::OutgoingCash).workflow_);
    workflow.set_id(workflowID.asBase58(api_.Crypto()));
    workflow.set_type(translate(PaymentWorkflowType::OutgoingCash));
    workflow.set_state(translate(PaymentWorkflowState::Unsent));
    auto& source = *(workflow.add_source());
    source.set_version(versions_.at(PaymentWorkflowType::OutgoingCash).source_);
    source.set_id(workflowID.asBase58(api_.Crypto()));
    source.set_revision(1);
    source.set_item([&] {
        auto proto = protobuf::Purse{};
        purse.Internal().Serialize(proto);

        return to_string(proto);
    }());
    workflow.set_notary(purse.Notary().asBase58(api_.Crypto()));
    auto& event = *workflow.add_event();
    event.set_version(versions_.at(PaymentWorkflowType::OutgoingCash).event_);
    event.set_time(seconds_since_epoch(Clock::now()).value());
    event.set_type(protobuf::PAYMENTEVENTTYPE_CREATE);
    event.set_method(protobuf::TRANSPORTMETHOD_NONE);
    event.set_success(true);
    workflow.add_unit(purse.Unit().asBase58(api_.Crypto()));
    const auto saved = save_workflow(id, workflow);

    if (false == saved) {
        LogError()()("Failed to save workflow").Flush();

        return {};
    }

    return workflowID;
}

auto Workflow::add_cheque_event(
    const eLock& lock,
    const identifier::Nym& nymID,
    const identifier::Nym&,
    protobuf::PaymentWorkflow& workflow,
    const PaymentWorkflowState newState,
    const protobuf::PaymentEventType newEventType,
    const VersionNumber version,
    const Message& request,
    const Message* reply,
    const identifier::Account& account) const -> bool
{
    const bool haveReply = (nullptr != reply);
    const bool success = cheque_deposit_success(reply);

    if (success) {
        workflow.set_state(translate(newState));

        if ((false == account.empty()) && (0 == workflow.account_size())) {
            workflow.add_account(account.asBase58(api_.Crypto()));
        }
    }

    auto& event = *(workflow.add_event());
    event.set_version(version);
    event.set_type(newEventType);
    event.add_item(String::Factory(request)->Get());
    event.set_method(protobuf::TRANSPORTMETHOD_OT);
    event.set_transport(request.notary_id_->Get());

    switch (newEventType) {
        case protobuf::PAYMENTEVENTTYPE_CANCEL:
        case protobuf::PAYMENTEVENTTYPE_COMPLETE: {
        } break;
        case protobuf::PAYMENTEVENTTYPE_CONVEY:
        case protobuf::PAYMENTEVENTTYPE_ACCEPT: {
            event.set_nym(request.nym_id2_->Get());
        } break;
        case protobuf::PAYMENTEVENTTYPE_ERROR:
        case protobuf::PAYMENTEVENTTYPE_CREATE:
        case protobuf::PAYMENTEVENTTYPE_ABORT:
        case protobuf::PAYMENTEVENTTYPE_ACKNOWLEDGE:
        case protobuf::PAYMENTEVENTTYPE_EXPIRE:
        case protobuf::PAYMENTEVENTTYPE_REJECT:
        default: {
            LogAbort()().Abort();
        }
    }

    event.set_success(success);

    if (haveReply) {
        event.add_item(String::Factory(*reply)->Get());
        event.set_time(reply->time_);
    } else {
        event.set_time(request.time_);
    }

    if (false == account.empty()) {
        workflow.set_notary(
            api_.Storage().Internal().AccountServer(account).asBase58(
                api_.Crypto()));
    }

    return save_workflow(nymID, account, workflow);
}

// Only used for ClearCheque
auto Workflow::add_cheque_event(
    const eLock& lock,
    const identifier::Nym& nymID,
    const identifier::Account& accountID,
    protobuf::PaymentWorkflow& workflow,
    const PaymentWorkflowState newState,
    const protobuf::PaymentEventType newEventType,
    const VersionNumber version,
    const identifier::Nym& recipientNymID,
    const OTTransaction& receipt,
    const Time time) const -> bool
{
    auto message = String::Factory();
    receipt.SaveContractRaw(message);
    workflow.set_state(translate(newState));
    auto& event = *(workflow.add_event());
    event.set_version(version);
    event.set_type(newEventType);
    event.add_item(message->Get());
    event.set_time(seconds_since_epoch(time).value());
    event.set_method(protobuf::TRANSPORTMETHOD_OT);
    event.set_transport(receipt.GetRealNotaryID().asBase58(api_.Crypto()));
    event.set_nym(recipientNymID.asBase58(api_.Crypto()));
    event.set_success(true);

    if (0 == workflow.party_size()) {
        workflow.add_party(recipientNymID.asBase58(api_.Crypto()));
    }

    return save_workflow(nymID, accountID, workflow);
}

auto Workflow::add_transfer_event(
    const eLock& lock,
    const identifier::Nym& nymID,
    const identifier::Nym& eventNym,
    protobuf::PaymentWorkflow& workflow,
    const PaymentWorkflowState newState,
    const protobuf::PaymentEventType newEventType,
    const VersionNumber version,
    const Message& message,
    const identifier::Account& account,
    const bool success) const -> bool
{
    if (success) { workflow.set_state(translate(newState)); }

    auto& event = *(workflow.add_event());
    event.set_version(version);
    event.set_type(newEventType);
    event.add_item(String::Factory(message)->Get());
    event.set_method(protobuf::TRANSPORTMETHOD_OT);
    event.set_transport(message.notary_id_->Get());

    switch (newEventType) {
        case protobuf::PAYMENTEVENTTYPE_CONVEY:
        case protobuf::PAYMENTEVENTTYPE_ACCEPT:
        case protobuf::PAYMENTEVENTTYPE_COMPLETE:
        case protobuf::PAYMENTEVENTTYPE_ABORT:
        case protobuf::PAYMENTEVENTTYPE_ACKNOWLEDGE: {
            // TODO
        } break;
        case protobuf::PAYMENTEVENTTYPE_ERROR:
        case protobuf::PAYMENTEVENTTYPE_CREATE:
        case protobuf::PAYMENTEVENTTYPE_CANCEL:
        case protobuf::PAYMENTEVENTTYPE_EXPIRE:
        case protobuf::PAYMENTEVENTTYPE_REJECT:
        default: {
            LogAbort()().Abort();
        }
    }

    event.set_success(success);
    event.set_time(message.time_);

    if (0 == workflow.party_size() && (false == eventNym.empty())) {
        workflow.add_party(eventNym.asBase58(api_.Crypto()));
    }

    return save_workflow(nymID, account, workflow);
}

auto Workflow::add_transfer_event(
    const eLock& lock,
    const identifier::Nym& nymID,
    const UnallocatedCString& notaryID,
    const identifier::Nym& eventNym,
    protobuf::PaymentWorkflow& workflow,
    const otx::client::PaymentWorkflowState newState,
    const protobuf::PaymentEventType newEventType,
    const VersionNumber version,
    const OTTransaction& receipt,
    const identifier::Account& account,
    const bool success) const -> bool
{
    if (success) { workflow.set_state(translate(newState)); }

    auto& event = *(workflow.add_event());
    event.set_version(version);
    event.set_type(newEventType);
    event.add_item(String::Factory(receipt)->Get());
    event.set_method(protobuf::TRANSPORTMETHOD_OT);
    event.set_transport(notaryID);

    switch (newEventType) {
        case protobuf::PAYMENTEVENTTYPE_CONVEY:
        case protobuf::PAYMENTEVENTTYPE_ACCEPT:
        case protobuf::PAYMENTEVENTTYPE_COMPLETE:
        case protobuf::PAYMENTEVENTTYPE_ABORT:
        case protobuf::PAYMENTEVENTTYPE_ACKNOWLEDGE: {
            // TODO
        } break;
        case protobuf::PAYMENTEVENTTYPE_ERROR:
        case protobuf::PAYMENTEVENTTYPE_CREATE:
        case protobuf::PAYMENTEVENTTYPE_CANCEL:
        case protobuf::PAYMENTEVENTTYPE_EXPIRE:
        case protobuf::PAYMENTEVENTTYPE_REJECT:
        default: {
            LogAbort()().Abort();
        }
    }

    event.set_success(success);
    event.set_time(seconds_since_epoch(Clock::now()).value());

    if (0 == workflow.party_size() && (false == eventNym.empty())) {
        workflow.add_party(eventNym.asBase58(api_.Crypto()));
    }

    return save_workflow(nymID, account, workflow);
}

auto Workflow::can_abort_transfer(const protobuf::PaymentWorkflow& workflow)
    -> bool
{
    bool correctState{false};

    switch (translate(workflow.state())) {
        case PaymentWorkflowState::Initiated: {
            correctState = true;
        } break;
        default: {
        }
    }

    if (false == correctState) {
        LogError()()("Incorrect workflow state.").Flush();

        return false;
    }

    return true;
}

auto Workflow::can_accept_cheque(const protobuf::PaymentWorkflow& workflow)
    -> bool
{
    bool correctState{false};

    switch (translate(workflow.state())) {
        case PaymentWorkflowState::Expired:
        case PaymentWorkflowState::Conveyed: {
            correctState = true;
        } break;
        default: {
        }
    }

    if (false == correctState) {
        LogError()()("Incorrect workflow state.").Flush();

        return false;
    }

    return true;
}

auto Workflow::can_accept_transfer(const protobuf::PaymentWorkflow& workflow)
    -> bool
{
    bool correctState{false};

    switch (translate(workflow.state())) {
        case PaymentWorkflowState::Conveyed: {
            correctState = true;
        } break;
        default: {
        }
    }

    if (false == correctState) {
        LogError()()("Incorrect workflow state.").Flush();

        return false;
    }

    return true;
}

auto Workflow::can_acknowledge_transfer(
    const protobuf::PaymentWorkflow& workflow) -> bool
{
    bool correctState{false};

    switch (translate(workflow.state())) {
        case PaymentWorkflowState::Initiated:
        case PaymentWorkflowState::Conveyed: {
            correctState = true;
        } break;
        default: {
        }
    }

    if (false == correctState) {
        LogError()()("Incorrect workflow state (")(workflow.state())(")")
            .Flush();

        return false;
    }

    return true;
}

auto Workflow::can_cancel_cheque(const protobuf::PaymentWorkflow& workflow)
    -> bool
{
    bool correctState{false};

    switch (translate(workflow.state())) {
        case PaymentWorkflowState::Unsent:
        case PaymentWorkflowState::Conveyed: {
            correctState = true;
        } break;
        default: {
        }
    }

    if (false == correctState) {
        LogError()()("Incorrect workflow state.").Flush();

        return false;
    }

    return true;
}

auto Workflow::can_clear_transfer(const protobuf::PaymentWorkflow& workflow)
    -> bool
{
    bool correctState{false};

    switch (translate(workflow.type())) {
        case PaymentWorkflowType::OutgoingTransfer: {
            correctState =
                (PaymentWorkflowState::Acknowledged ==
                 translate(workflow.state()));
        } break;
        case PaymentWorkflowType::InternalTransfer: {
            correctState =
                (PaymentWorkflowState::Conveyed == translate(workflow.state()));
        } break;
        default: {
        }
    }

    if (false == correctState) {
        LogError()()("Incorrect workflow state.").Flush();

        return false;
    }

    return true;
}

auto Workflow::can_complete_transfer(const protobuf::PaymentWorkflow& workflow)
    -> bool
{
    if (PaymentWorkflowState::Accepted != translate(workflow.state())) {
        LogError()()("Incorrect workflow state (")(workflow.state())(")")
            .Flush();

        return false;
    }

    return true;
}

auto Workflow::can_convey_cash(const protobuf::PaymentWorkflow& workflow)
    -> bool
{
    if (PaymentWorkflowState::Expired == translate(workflow.state())) {
        LogError()()("Incorrect workflow state.").Flush();

        return false;
    }

    return true;
}

auto Workflow::can_convey_cheque(const protobuf::PaymentWorkflow& workflow)
    -> bool
{
    if (PaymentWorkflowState::Unsent != translate(workflow.state())) {
        LogError()()("Incorrect workflow state.").Flush();

        return false;
    }

    return true;
}

auto Workflow::can_convey_transfer(const protobuf::PaymentWorkflow& workflow)
    -> bool
{
    switch (translate(workflow.state())) {
        case PaymentWorkflowState::Initiated:
        case PaymentWorkflowState::Acknowledged: {
            return true;
        }
        case PaymentWorkflowState::Conveyed: {
            break;
        }
        default: {
            LogError()()("Incorrect workflow state.").Flush();
        }
    }

    return false;
}

auto Workflow::can_deposit_cheque(const protobuf::PaymentWorkflow& workflow)
    -> bool
{
    if (PaymentWorkflowState::Conveyed != translate(workflow.state())) {
        LogError()()("Incorrect workflow state.").Flush();

        return false;
    }

    return true;
}

auto Workflow::can_expire_cheque(
    const opentxs::Cheque& cheque,
    const protobuf::PaymentWorkflow& workflow) -> bool
{
    bool correctState{false};

    switch (translate(workflow.type())) {
        case PaymentWorkflowType::OutgoingCheque: {
            switch (translate(workflow.state())) {
                case PaymentWorkflowState::Unsent:
                case PaymentWorkflowState::Conveyed: {
                    correctState = true;
                } break;
                default: {
                }
            }
        } break;
        case PaymentWorkflowType::IncomingCheque: {
            switch (translate(workflow.state())) {
                case PaymentWorkflowState::Conveyed: {
                    correctState = true;
                } break;
                default: {
                }
            }
        } break;
        default: {
            LogAbort()().Abort();
        }
    }

    if (false == correctState) {
        LogError()()("Incorrect workflow state.").Flush();

        return false;
    }

    if (Clock::now() < cheque.GetValidTo()) {
        LogError()()("Can not expire valid cheque.").Flush();

        return false;
    }

    return true;
}

auto Workflow::can_finish_cheque(const protobuf::PaymentWorkflow& workflow)
    -> bool
{
    if (PaymentWorkflowState::Accepted != translate(workflow.state())) {
        LogError()()("Incorrect workflow state.").Flush();

        return false;
    }

    return true;
}

auto Workflow::CancelCheque(
    const opentxs::Cheque& cheque,
    const Message& request,
    const Message* reply) const -> bool
{
    if (false == isCheque(cheque)) { return false; }

    const auto& nymID = cheque.GetSenderNymID();
    auto global = Lock{lock_};
    const auto workflow = get_workflow(
        global, {PaymentWorkflowType::OutgoingCheque}, nymID, cheque);

    if (false == bool(workflow)) {
        LogError()()("Workflow for this cheque does not exist.").Flush();

        return false;
    }

    auto lock = get_workflow_lock(global, workflow->id());

    if (false == can_cancel_cheque(*workflow)) { return false; }

    static const auto accountID = identifier::Account{};

    return add_cheque_event(
        lock,
        nymID,
        {},
        *workflow,
        PaymentWorkflowState::Cancelled,
        protobuf::PAYMENTEVENTTYPE_CANCEL,
        versions_.at(PaymentWorkflowType::OutgoingCheque).event_,
        request,
        reply,
        accountID);
}

auto Workflow::cheque_deposit_success(const Message* message) -> bool
{
    if (nullptr == message) { return false; }

    // TODO this might not be sufficient

    return message->success_;
}

auto Workflow::ClearCheque(
    const identifier::Nym& recipientNymID,
    const OTTransaction& receipt) const -> bool
{
    if (recipientNymID.empty()) {
        LogError()()("Invalid cheque recipient").Flush();

        return false;
    }

    auto cheque{api_.Factory().Internal().Session().Cheque(receipt)};

    if (false == bool(cheque)) {
        LogError()()("Failed to load cheque from receipt.").Flush();

        return false;
    }

    if (false == isCheque(*cheque)) { return false; }

    const auto& nymID = cheque->GetSenderNymID();
    auto global = Lock{lock_};
    const auto workflow = get_workflow(
        global, {PaymentWorkflowType::OutgoingCheque}, nymID, *cheque);

    if (false == bool(workflow)) {
        LogError()()("Workflow for this cheque does not exist.").Flush();

        return false;
    }

    auto lock = get_workflow_lock(global, workflow->id());
    const auto workflowID = api_.Factory().IdentifierFromBase58(workflow->id());

    if (false == can_accept_cheque(*workflow)) { return false; }

    assert_true(1 == workflow->account_size());

    const bool needNym = (0 == workflow->party_size());
    const auto time = Clock::now();
    const auto output = add_cheque_event(
        lock,
        nymID,
        api_.Factory().AccountIDFromBase58(workflow->account(0)),
        *workflow,
        PaymentWorkflowState::Accepted,
        protobuf::PAYMENTEVENTTYPE_ACCEPT,
        versions_.at(PaymentWorkflowType::OutgoingCheque).event_,
        recipientNymID,
        receipt,
        time);

    if (needNym) {
        update_activity(
            cheque->GetSenderNymID(),
            recipientNymID,
            api_.Factory().Internal().Identifier(*cheque),
            workflowID,
            otx::client::StorageBox::OUTGOINGCHEQUE,
            extract_conveyed_time(*workflow));
    }

    update_rpc(
        nymID,
        cheque->GetRecipientNymID(),
        cheque->SourceAccountID().asBase58(api_.Crypto()),
        protobuf::ACCOUNTEVENT_OUTGOINGCHEQUE,
        workflowID,
        -1 * cheque->GetAmount(),
        0,
        time,
        cheque->GetMemo().Get());

    return output;
}

auto Workflow::ClearTransfer(
    const identifier::Nym& nymID,
    const identifier::Notary& notaryID,
    const OTTransaction& receipt) const -> bool
{
    auto depositorNymID = identifier::Nym{};
    const auto transfer =
        extract_transfer_from_receipt(receipt, depositorNymID);

    if (false == bool(transfer)) {
        LogError()()("Invalid transfer").Flush();

        return false;
    }

    if (depositorNymID.empty()) {
        LogError()()("Missing recipient").Flush();

        return false;
    }

    contact_.NymToContact(depositorNymID);
    const auto& accountID = transfer->GetPurportedAccountID();

    if (accountID.empty()) {
        LogError()()("Transfer does not contain source account ID").Flush();

        return false;
    }

    const auto& destinationAccountID = transfer->GetDestinationAcctID();

    if (destinationAccountID.empty()) {
        LogError()()("Transfer does not contain destination account ID")
            .Flush();

        return false;
    }

    const bool isInternal = isInternalTransfer(accountID, destinationAccountID);
    const UnallocatedSet<PaymentWorkflowType> type{
        isInternal ? PaymentWorkflowType::InternalTransfer
                   : PaymentWorkflowType::OutgoingTransfer};
    auto global = Lock{lock_};
    const auto workflow = get_workflow(global, type, nymID, *transfer);

    if (false == bool(workflow)) {
        LogError()()("Workflow for this transfer does not exist.").Flush();

        return false;
    }

    auto lock = get_workflow_lock(global, workflow->id());
    const auto workflowID = api_.Factory().IdentifierFromBase58(workflow->id());

    if (false == can_clear_transfer(*workflow)) { return false; }

    const auto output = add_transfer_event(
        lock,
        nymID,
        notaryID.asBase58(api_.Crypto()),
        (isInternal ? identifier::Nym{} : depositorNymID),
        *workflow,
        PaymentWorkflowState::Accepted,
        protobuf::PAYMENTEVENTTYPE_ACCEPT,
        (isInternal
             ? versions_.at(PaymentWorkflowType::InternalTransfer).event_
             : versions_.at(PaymentWorkflowType::OutgoingTransfer).event_),
        receipt,
        accountID,
        true);

    if (output) {
        const auto time = extract_conveyed_time(*workflow);
        auto note = String::Factory();
        transfer->GetNote(note);
        update_activity(
            nymID,
            depositorNymID,
            api_.Factory().Internal().Identifier(*transfer),
            workflowID,
            otx::client::StorageBox::OUTGOINGTRANSFER,
            time);
        update_rpc(
            nymID,
            depositorNymID,
            accountID.asBase58(api_.Crypto()),
            protobuf::ACCOUNTEVENT_OUTGOINGTRANSFER,
            workflowID,
            transfer->GetAmount(),
            0,
            time,
            note->Get());
    }

    return output;
}

// Works for outgoing and internal transfer workflows.
auto Workflow::CompleteTransfer(
    const identifier::Nym& nymID,
    const identifier::Notary& notaryID,
    const OTTransaction& receipt,
    const Message& reply) const -> bool
{
    auto depositorNymID = identifier::Nym{};
    const auto transfer =
        extract_transfer_from_receipt(receipt, depositorNymID);

    if (false == bool(transfer)) {
        LogError()()("Invalid transfer").Flush();

        return false;
    }

    const auto& accountID = transfer->GetPurportedAccountID();

    if (accountID.empty()) {
        LogError()()("Transfer does not contain source account ID").Flush();

        return false;
    }

    const auto& destinationAccountID = transfer->GetDestinationAcctID();

    if (destinationAccountID.empty()) {
        LogError()()("Transfer does not contain destination account ID")
            .Flush();

        return false;
    }

    const bool isInternal = isInternalTransfer(accountID, destinationAccountID);
    const UnallocatedSet<PaymentWorkflowType> type{
        isInternal ? PaymentWorkflowType::InternalTransfer
                   : PaymentWorkflowType::OutgoingTransfer};
    auto global = Lock{lock_};
    const auto workflow = get_workflow(global, type, nymID, *transfer);

    if (false == bool(workflow)) {
        LogError()()("Workflow for this transfer does not exist.").Flush();

        return false;
    }

    auto lock = get_workflow_lock(global, workflow->id());

    if (false == can_complete_transfer(*workflow)) { return false; }

    return add_transfer_event(
        lock,
        nymID,
        notaryID.asBase58(api_.Crypto()),
        (isInternal ? identifier::Nym{} : depositorNymID),
        *workflow,
        PaymentWorkflowState::Completed,
        protobuf::PAYMENTEVENTTYPE_COMPLETE,
        (isInternal
             ? versions_.at(PaymentWorkflowType::InternalTransfer).event_
             : versions_.at(PaymentWorkflowType::OutgoingTransfer).event_),
        receipt,
        transfer->GetRealAccountID(),
        true);
}

// NOTE: Since this is an INCOMING transfer, then we need to CREATE its
// corresponding transfer workflow, since it does not already exist.
//
// (Whereas if this had been an INTERNAL transfer, then it would ALREADY
// have been created, and thus we'd need to GET the existing workflow, and
// then add the new event to it).
auto Workflow::convey_incoming_transfer(
    const identifier::Nym& nymID,
    const identifier::Notary& notaryID,
    const OTTransaction& pending,
    const identifier::Nym& senderNymID,
    const identifier::Nym& recipientNymID,
    const Item& transfer) const -> identifier::Generic
{
    const auto global = Lock{lock_};
    const auto existing = get_workflow(
        global, {PaymentWorkflowType::IncomingTransfer}, nymID, transfer);

    if (existing) {
        LogError()()("Workflow for this transfer already exist.").Flush();

        return api_.Factory().IdentifierFromBase58(existing->id());
    }

    const auto& accountID = pending.GetPurportedAccountID();
    const auto [workflowID, workflow] = create_transfer(
        global,
        nymID,
        transfer,
        PaymentWorkflowType::IncomingTransfer,
        PaymentWorkflowState::Conveyed,
        versions_.at(PaymentWorkflowType::IncomingTransfer).workflow_,
        versions_.at(PaymentWorkflowType::IncomingTransfer).source_,
        versions_.at(PaymentWorkflowType::IncomingTransfer).event_,
        senderNymID,
        accountID,
        notaryID.asBase58(api_.Crypto()),
        "");

    if (false == workflowID.empty()) {
        const auto time = extract_conveyed_time(workflow);
        auto note = String::Factory();
        transfer.GetNote(note);
        update_activity(
            nymID,
            transfer.GetNymID(),
            api_.Factory().Internal().Identifier(transfer),
            workflowID,
            otx::client::StorageBox::INCOMINGTRANSFER,
            time);
        update_rpc(
            recipientNymID,
            senderNymID,
            accountID.asBase58(api_.Crypto()),
            protobuf::ACCOUNTEVENT_INCOMINGTRANSFER,
            workflowID,
            transfer.GetAmount(),
            0,
            time,
            note->Get());
    }

    return workflowID;
}

// NOTE: Since this is an INTERNAL transfer, then it was already CREATED,
// and thus we need to GET the existing workflow, and then add the new
// event to it.
// Whereas if this is an INCOMING transfer, then we need to CREATE its
// corresponding transfer workflow since it does not already exist.
auto Workflow::convey_internal_transfer(
    const identifier::Nym& nymID,
    const identifier::Notary& notaryID,
    const OTTransaction& pending,
    const identifier::Nym& senderNymID,
    const Item& transfer) const -> identifier::Generic
{
    auto global = Lock{lock_};
    const auto workflow = get_workflow(
        global, {PaymentWorkflowType::InternalTransfer}, nymID, transfer);

    if (false == bool(workflow)) {
        LogError()()("Workflow for this transfer does not exist.").Flush();

        return {};
    }

    auto lock = get_workflow_lock(global, workflow->id());

    if (false == can_convey_transfer(*workflow)) { return {}; }

    const auto output = add_transfer_event(
        lock,
        nymID,
        notaryID.asBase58(api_.Crypto()),
        {},
        *workflow,
        PaymentWorkflowState::Conveyed,
        protobuf::PAYMENTEVENTTYPE_CONVEY,
        versions_.at(PaymentWorkflowType::InternalTransfer).event_,
        pending,
        transfer.GetDestinationAcctID(),
        true);

    if (output) {
        return api_.Factory().IdentifierFromBase58(workflow->id());
    } else {
        return {};
    }
}

auto Workflow::ConveyTransfer(
    const identifier::Nym& nymID,
    const identifier::Notary& notaryID,
    const OTTransaction& pending) const -> identifier::Generic
{
    const auto transfer = extract_transfer_from_pending(pending);

    if (false == bool(transfer)) {
        LogError()()("Invalid transaction").Flush();

        return {};
    }

    const auto& senderNymID = transfer->GetNymID();
    contact_.NymToContact(transfer->GetNymID());
    const auto& recipientNymID = pending.GetNymID();

    if (pending.GetNymID() != nymID) {
        LogError()()("Invalid recipient").Flush();

        return {};
    }

    const bool isInternal = (senderNymID == recipientNymID);

    if (isInternal) {
        return convey_internal_transfer(
            nymID, notaryID, pending, senderNymID, *transfer);
    } else {
        return convey_incoming_transfer(

            nymID, notaryID, pending, senderNymID, recipientNymID, *transfer);
    }
}

auto Workflow::create_cheque(
    const Lock& lock,
    const identifier::Nym& nymID,
    const opentxs::Cheque& cheque,
    const PaymentWorkflowType workflowType,
    const PaymentWorkflowState workflowState,
    const VersionNumber workflowVersion,
    const VersionNumber sourceVersion,
    const VersionNumber eventVersion,
    const identifier::Nym& party,
    const identifier::Account& account,
    const Message* message) const
    -> std::pair<identifier::Generic, protobuf::PaymentWorkflow>
{
    assert_true(verify_lock(lock));

    auto output = std::pair<identifier::Generic, protobuf::PaymentWorkflow>{};
    auto& [workflowID, workflow] = output;
    const auto chequeID = api_.Factory().Internal().Identifier(cheque);
    const UnallocatedCString serialized = String::Factory(cheque)->Get();
    workflowID = api_.Factory().IdentifierFromRandom();
    workflow.set_version(workflowVersion);
    workflow.set_id(workflowID.asBase58(api_.Crypto()));
    workflow.set_type(translate(workflowType));
    workflow.set_state(translate(workflowState));
    auto& source = *(workflow.add_source());
    source.set_version(sourceVersion);
    source.set_id(chequeID.asBase58(api_.Crypto()));
    source.set_revision(1);
    source.set_item(serialized);

    // add party if it was passed in and is not already present
    if ((false == party.empty()) && (0 == workflow.party_size())) {
        workflow.add_party(party.asBase58(api_.Crypto()));
    }

    auto& event = *workflow.add_event();
    event.set_version(eventVersion);

    if (nullptr != message) {
        event.set_type(protobuf::PAYMENTEVENTTYPE_CONVEY);
        event.add_item(String::Factory(*message)->Get());
        event.set_time(message->time_);
        event.set_method(protobuf::TRANSPORTMETHOD_OT);
        event.set_transport(message->notary_id_->Get());
    } else {
        event.set_time(seconds_since_epoch(Clock::now()).value());

        if (PaymentWorkflowState::Unsent == workflowState) {
            event.set_type(protobuf::PAYMENTEVENTTYPE_CREATE);
            event.set_method(protobuf::TRANSPORTMETHOD_NONE);
        } else if (PaymentWorkflowState::Conveyed == workflowState) {
            event.set_type(protobuf::PAYMENTEVENTTYPE_CONVEY);
            event.set_method(protobuf::TRANSPORTMETHOD_OOB);
        } else {
            LogAbort()().Abort();
        }
    }

    if (false == party.empty()) {
        event.set_nym(party.asBase58(api_.Crypto()));
    }

    event.set_success(true);
    workflow.add_unit(
        cheque.GetInstrumentDefinitionID().asBase58(api_.Crypto()));

    // add account if it was passed in and is not already present
    if ((false == account.empty()) && (0 == workflow.account_size())) {
        workflow.add_account(account.asBase58(api_.Crypto()));
    }

    if ((false == account.empty()) && (workflow.notary().empty())) {
        workflow.set_notary(
            api_.Storage().Internal().AccountServer(account).asBase58(
                api_.Crypto()));
    }

    if (workflow.notary().empty() && (nullptr != message)) {
        workflow.set_notary(message->notary_id_->Get());
    }

    return save_workflow(std::move(output), nymID, account, workflow);
}

auto Workflow::create_transfer(
    const Lock& global,
    const identifier::Nym& nymID,
    const Item& transfer,
    const PaymentWorkflowType workflowType,
    const PaymentWorkflowState workflowState,
    const VersionNumber workflowVersion,
    const VersionNumber sourceVersion,
    const VersionNumber eventVersion,
    const identifier::Nym& party,
    const identifier::Account& account,
    const UnallocatedCString& notaryID,
    const UnallocatedCString& destinationAccountID) const
    -> std::pair<identifier::Generic, protobuf::PaymentWorkflow>
{
    assert_true(verify_lock(global));
    assert_false(nymID.empty());
    assert_false(account.empty());
    assert_false(notaryID.empty());

    auto output = std::pair<identifier::Generic, protobuf::PaymentWorkflow>{};
    auto& [workflowID, workflow] = output;
    const auto transferID = api_.Factory().Internal().Identifier(transfer);
    LogVerbose()()("Transfer ID: ")(transferID, api_.Crypto()).Flush();
    const UnallocatedCString serialized = String::Factory(transfer)->Get();
    const auto existing = get_workflow(global, {workflowType}, nymID, transfer);

    if (existing) {
        LogError()()("Workflow for this transfer already exists.").Flush();
        workflowID = api_.Factory().IdentifierFromBase58(existing->id());

        return output;
    }

    workflowID = api_.Factory().IdentifierFromRandom();
    workflow.set_version(workflowVersion);
    workflow.set_id(workflowID.asBase58(api_.Crypto()));
    workflow.set_type(translate(workflowType));
    workflow.set_state(translate(workflowState));
    auto& source = *(workflow.add_source());
    source.set_version(sourceVersion);
    source.set_id(transferID.asBase58(api_.Crypto()));
    source.set_revision(1);
    source.set_item(serialized);
    workflow.set_notary(notaryID);

    // add party if it was passed in and is not already present
    if ((false == party.empty()) && (0 == workflow.party_size())) {
        workflow.add_party(party.asBase58(api_.Crypto()));
    }

    auto& event = *workflow.add_event();
    event.set_version(eventVersion);
    event.set_time(seconds_since_epoch(Clock::now()).value());

    if (PaymentWorkflowState::Initiated == workflowState) {
        event.set_type(protobuf::PAYMENTEVENTTYPE_CREATE);
        event.set_method(protobuf::TRANSPORTMETHOD_OT);
    } else if (PaymentWorkflowState::Conveyed == workflowState) {
        event.set_type(protobuf::PAYMENTEVENTTYPE_CONVEY);
        event.set_method(protobuf::TRANSPORTMETHOD_OT);
    } else {
        LogAbort()().Abort();
    }

    event.set_transport(notaryID);

    if (false == party.empty()) {
        event.set_nym(party.asBase58(api_.Crypto()));
    }

    event.set_success(true);
    workflow.add_unit(
        api_.Storage().Internal().AccountContract(account).asBase58(
            api_.Crypto()));

    // add account if it is not already present
    if (0 == workflow.account_size()) {
        workflow.add_account(account.asBase58(api_.Crypto()));

        if (false == destinationAccountID.empty()) {
            workflow.add_account(destinationAccountID);
        }
    }

    return save_workflow(std::move(output), nymID, account, workflow);
}

// Creates outgoing and internal transfer workflows.
auto Workflow::CreateTransfer(const Item& transfer, const Message& request)
    const -> identifier::Generic
{
    if (false == isTransfer(transfer)) {
        LogError()()("Invalid item type on object").Flush();

        return {};
    }

    const auto senderNymID =
        api_.Factory().NymIDFromBase58(request.nym_id_->Bytes());
    const auto& accountID = transfer.GetRealAccountID();
    const bool isInternal =
        isInternalTransfer(accountID, transfer.GetDestinationAcctID());
    const auto global = Lock{lock_};
    const auto existing = get_workflow(
        global,
        {isInternal ? PaymentWorkflowType::InternalTransfer
                    : PaymentWorkflowType::OutgoingTransfer},
        senderNymID,
        transfer);

    if (existing) {
        LogError()()("Workflow for this transfer already exist.").Flush();

        return api_.Factory().IdentifierFromBase58(existing->id());
    }

    const auto [workflowID, workflow] = create_transfer(
        global,
        senderNymID,
        transfer,
        (isInternal ? PaymentWorkflowType::InternalTransfer
                    : PaymentWorkflowType::OutgoingTransfer),
        PaymentWorkflowState::Initiated,
        (isInternal
             ? versions_.at(PaymentWorkflowType::InternalTransfer).workflow_
             : versions_.at(PaymentWorkflowType::OutgoingTransfer).workflow_),
        (isInternal
             ? versions_.at(PaymentWorkflowType::InternalTransfer).source_
             : versions_.at(PaymentWorkflowType::OutgoingTransfer).source_),
        (isInternal
             ? versions_.at(PaymentWorkflowType::InternalTransfer).event_
             : versions_.at(PaymentWorkflowType::OutgoingTransfer).event_),
        {},
        accountID,
        request.notary_id_->Get(),
        (isInternal ? transfer.GetDestinationAcctID().asBase58(api_.Crypto())
                    : ""));

    if (false == workflowID.empty()) {
        const auto time = extract_conveyed_time(workflow);
        auto note = String::Factory();
        transfer.GetNote(note);
        update_rpc(
            senderNymID,
            {},
            accountID.asBase58(api_.Crypto()),
            protobuf::ACCOUNTEVENT_OUTGOINGTRANSFER,
            workflowID,
            transfer.GetAmount(),
            0,
            time,
            note->Get());
    }

    return workflowID;
}

auto Workflow::DepositCheque(
    const identifier::Nym& receiver,
    const identifier::Account& accountID,
    const opentxs::Cheque& cheque,
    const Message& request,
    const Message* reply) const -> bool
{
    if (false == isCheque(cheque)) { return false; }

    auto global = Lock{lock_};
    const auto workflow = get_workflow(
        global, {PaymentWorkflowType::IncomingCheque}, receiver, cheque);

    if (false == bool(workflow)) {
        LogError()()("Workflow for this cheque does not exist.").Flush();

        return false;
    }

    auto lock = get_workflow_lock(global, workflow->id());

    if (false == can_deposit_cheque(*workflow)) { return false; }

    const auto output = add_cheque_event(
        lock,
        receiver,
        cheque.GetSenderNymID(),
        *workflow,
        PaymentWorkflowState::Completed,
        protobuf::PAYMENTEVENTTYPE_ACCEPT,
        versions_.at(PaymentWorkflowType::IncomingCheque).event_,
        request,
        reply,
        accountID);

    if (output && cheque_deposit_success(reply)) {
        update_rpc(
            receiver,
            cheque.GetSenderNymID(),
            accountID.asBase58(api_.Crypto()),
            protobuf::ACCOUNTEVENT_INCOMINGCHEQUE,
            api_.Factory().IdentifierFromBase58(workflow->id()),
            cheque.GetAmount(),
            0,
            seconds_since_epoch_unsigned(reply->time_).value(),
            cheque.GetMemo().Get());
    }

    return output;
}

auto Workflow::ExpireCheque(
    const identifier::Nym& nym,
    const opentxs::Cheque& cheque) const -> bool
{
    if (false == isCheque(cheque)) { return false; }

    auto global = Lock{lock_};
    const auto workflow = get_workflow(
        global,
        {PaymentWorkflowType::OutgoingCheque,
         PaymentWorkflowType::IncomingCheque},
        nym,
        cheque);

    if (false == bool(workflow)) {
        LogError()()("Workflow for this cheque does not exist.").Flush();

        return false;
    }

    auto lock = get_workflow_lock(global, workflow->id());

    if (false == can_expire_cheque(cheque, *workflow)) { return false; }

    workflow->set_state(translate(PaymentWorkflowState::Expired));

    return save_workflow(nym, cheque.GetSenderAcctID(), *workflow);
}

auto Workflow::ExportCheque(const opentxs::Cheque& cheque) const -> bool
{
    if (false == isCheque(cheque)) { return false; }

    const auto& nymID = cheque.GetSenderNymID();
    auto global = Lock{lock_};
    const auto workflow = get_workflow(global, {}, nymID, cheque);

    if (false == bool(workflow)) {
        LogError()()("Workflow for this cheque does not exist.").Flush();

        return false;
    }

    auto lock = get_workflow_lock(global, workflow->id());

    if (false == can_convey_cheque(*workflow)) { return false; }

    workflow->set_state(translate(PaymentWorkflowState::Conveyed));
    auto& event = *(workflow->add_event());
    event.set_version(versions_.at(PaymentWorkflowType::OutgoingCheque).event_);
    event.set_type(protobuf::PAYMENTEVENTTYPE_CONVEY);
    event.set_time(seconds_since_epoch(Clock::now()).value());
    event.set_method(protobuf::TRANSPORTMETHOD_OOB);
    event.set_success(true);

    return save_workflow(nymID, cheque.GetSenderAcctID(), *workflow);
}

auto Workflow::extract_conveyed_time(const protobuf::PaymentWorkflow& workflow)
    -> Time
{
    for (const auto& event : workflow.event()) {
        if (protobuf::PAYMENTEVENTTYPE_CONVEY == event.type()) {
            if (event.success()) {
                return seconds_since_epoch_unsigned(event.time()).value();
            }
        }
    }

    return {};
}

auto Workflow::extract_transfer_from_pending(const OTTransaction& receipt) const
    -> std::unique_ptr<Item>
{
    if (otx::transactionType::pending != receipt.GetType()) {
        LogError()()("Incorrect receipt type: ")(receipt.GetTypeString())
            .Flush();

        return nullptr;
    }

    auto serializedTransfer = String::Factory();
    receipt.GetReferenceString(serializedTransfer);

    if (serializedTransfer->empty()) {
        LogError()()("Missing serialized transfer item").Flush();

        return nullptr;
    }

    auto transfer =
        api_.Factory().Internal().Session().Item(serializedTransfer);

    if (false == bool(transfer)) {
        LogError()()("Unable to instantiate transfer item").Flush();

        return nullptr;
    }

    if (otx::itemType::transfer != transfer->GetType()) {
        LogError()()("Invalid transfer item type.").Flush();

        return nullptr;
    }

    return transfer;
}

auto Workflow::extract_transfer_from_receipt(
    const OTTransaction& receipt,
    identifier::Nym& depositorNymID) const -> std::unique_ptr<Item>
{
    if (otx::transactionType::transferReceipt != receipt.GetType()) {
        if (otx::transactionType::pending == receipt.GetType()) {
            return extract_transfer_from_pending(receipt);
        } else {
            LogError()()("Incorrect receipt type: ")(receipt.GetTypeString())
                .Flush();

            return nullptr;
        }
    }

    auto serializedAcceptPending = String::Factory();
    receipt.GetReferenceString(serializedAcceptPending);

    if (serializedAcceptPending->empty()) {
        LogError()()("Missing serialized accept pending item").Flush();

        return nullptr;
    }

    const auto acceptPending =
        api_.Factory().Internal().Session().Item(serializedAcceptPending);

    if (false == bool(acceptPending)) {
        LogError()()("Unable to instantiate accept pending item").Flush();

        return nullptr;
    }

    if (otx::itemType::acceptPending != acceptPending->GetType()) {
        LogError()()("Invalid accept pending item type.").Flush();

        return nullptr;
    }

    depositorNymID = acceptPending->GetNymID();
    auto serializedPending = String::Factory();
    acceptPending->GetAttachment(serializedPending);

    if (serializedPending->empty()) {
        LogError()()("Missing serialized pending transaction").Flush();

        return nullptr;
    }

    auto pending = api_.Factory().Internal().Session().Transaction(
        receipt.GetNymID(),
        receipt.GetRealAccountID(),
        receipt.GetRealNotaryID());

    if (false == bool(pending)) {
        LogError()()("Unable to instantiate pending transaction").Flush();

        return nullptr;
    }

    const bool loaded = pending->LoadContractFromString(serializedPending);

    if (false == loaded) {
        LogError()()("Unable to deserialize pending transaction").Flush();

        return nullptr;
    }

    if (otx::transactionType::pending != pending->GetType()) {
        LogError()()("Invalid pending transaction type.").Flush();

        return nullptr;
    }

    auto serializedTransfer = String::Factory();
    pending->GetReferenceString(serializedTransfer);

    if (serializedTransfer->empty()) {
        LogError()()("Missing serialized transfer item").Flush();

        return nullptr;
    }

    auto transfer =
        api_.Factory().Internal().Session().Item(serializedTransfer);

    if (false == bool(transfer)) {
        LogError()()("Unable to instantiate transfer item").Flush();

        return nullptr;
    }

    if (otx::itemType::transfer != transfer->GetType()) {
        LogError()()("Invalid transfer item type.").Flush();

        return nullptr;
    }

    return transfer;
}

auto Workflow::FinishCheque(
    const opentxs::Cheque& cheque,
    const Message& request,
    const Message* reply) const -> bool
{
    if (false == isCheque(cheque)) { return false; }

    const auto& nymID = cheque.GetSenderNymID();
    auto global = Lock{lock_};
    const auto workflow = get_workflow(
        global, {PaymentWorkflowType::OutgoingCheque}, nymID, cheque);

    if (false == bool(workflow)) {
        LogError()()("Workflow for this cheque does not exist.").Flush();

        return false;
    }

    auto lock = get_workflow_lock(global, workflow->id());

    if (false == can_finish_cheque(*workflow)) { return false; }

    static const auto accountID = identifier::Account{};

    return add_cheque_event(
        lock,
        nymID,
        {},
        *workflow,
        PaymentWorkflowState::Completed,
        protobuf::PAYMENTEVENTTYPE_COMPLETE,
        versions_.at(PaymentWorkflowType::OutgoingCheque).event_,
        request,
        reply,
        accountID);
}

template <typename T>
auto Workflow::get_workflow(
    const Lock& global,
    const UnallocatedSet<PaymentWorkflowType>& types,
    const identifier::Nym& nymID,
    const T& source) const -> std::shared_ptr<protobuf::PaymentWorkflow>
{
    assert_true(verify_lock(global));

    const auto itemID = api_.Factory().Internal().Identifier(source);
    LogVerbose()()("Item ID: ")(itemID, api_.Crypto()).Flush();

    return get_workflow_by_source(types, nymID, itemID);
}

auto Workflow::get_workflow_by_id(
    const identifier::Nym& nymID,
    const identifier::Generic& workflowID) const
    -> std::shared_ptr<protobuf::PaymentWorkflow>
{
    auto output = std::make_shared<protobuf::PaymentWorkflow>();

    assert_false(nullptr == output);

    if (false == api_.Storage().Internal().Load(nymID, workflowID, *output)) {
        LogDetail()()("Workflow ")(workflowID, api_.Crypto())(" for nym ")(
            nymID, api_.Crypto())(" can not be loaded")
            .Flush();

        return {};
    }

    return output;
}

auto Workflow::get_workflow_by_id(
    const UnallocatedSet<PaymentWorkflowType>& types,
    const identifier::Nym& nymID,
    const identifier::Generic& workflowID) const
    -> std::shared_ptr<protobuf::PaymentWorkflow>
{
    auto output = get_workflow_by_id(nymID, workflowID);

    if (false == types.contains(translate(output->type()))) {
        LogError()()("Incorrect type (")(output->type())(") on workflow ")(
            workflowID, api_.Crypto())(" for nym ")(nymID, api_.Crypto())
            .Flush();

        return {nullptr};
    }

    return output;
}

auto Workflow::get_workflow_by_source(
    const UnallocatedSet<PaymentWorkflowType>& types,
    const identifier::Nym& nymID,
    const identifier::Generic& sourceID) const
    -> std::shared_ptr<protobuf::PaymentWorkflow>
{
    const auto workflowID =
        api_.Storage().Internal().PaymentWorkflowLookup(nymID, sourceID);

    if (workflowID.empty()) { return {}; }

    return get_workflow_by_id(types, nymID, workflowID);
}

auto Workflow::get_workflow_lock(Lock& global, const UnallocatedCString& id)
    const -> eLock
{
    assert_true(verify_lock(global));

    auto output = eLock(workflow_locks_[id]);
    global.unlock();

    return output;
}

auto Workflow::ImportCheque(
    const identifier::Nym& nymID,
    const opentxs::Cheque& cheque) const -> identifier::Generic
{
    if (false == isCheque(cheque)) { return {}; }

    if (false == validate_recipient(nymID, cheque)) {
        LogError()()("Nym ")(nymID, api_.Crypto())(
            " can not deposit this cheque.")
            .Flush();

        return {};
    }

    const auto global = Lock{lock_};
    const auto existing = get_workflow(
        global, {PaymentWorkflowType::IncomingCheque}, nymID, cheque);

    if (existing) {
        LogError()()("Workflow for this cheque already exist.").Flush();

        return api_.Factory().IdentifierFromBase58(existing->id());
    }

    const auto& party = cheque.GetSenderNymID();
    static const auto accountID = identifier::Account{};
    const auto [workflowID, workflow] = create_cheque(
        global,
        nymID,
        cheque,
        PaymentWorkflowType::IncomingCheque,
        PaymentWorkflowState::Conveyed,
        versions_.at(PaymentWorkflowType::IncomingCheque).workflow_,
        versions_.at(PaymentWorkflowType::IncomingCheque).source_,
        versions_.at(PaymentWorkflowType::IncomingCheque).event_,
        party,
        accountID);

    if (false == workflowID.empty()) {
        const auto time = extract_conveyed_time(workflow);
        update_activity(
            nymID,
            cheque.GetSenderNymID(),
            api_.Factory().Internal().Identifier(cheque),
            workflowID,
            otx::client::StorageBox::INCOMINGCHEQUE,
            time);
        update_rpc(
            nymID,
            cheque.GetSenderNymID(),
            {},
            protobuf::ACCOUNTEVENT_INCOMINGCHEQUE,
            workflowID,
            0,
            cheque.GetAmount(),
            time,
            cheque.GetMemo().Get());
    }

    return workflowID;
}

auto Workflow::InstantiateCheque(
    const identifier::Nym& nym,
    const identifier::Generic& id) const -> Cheque
{
    try {
        const auto workflow = [&] {
            auto out = protobuf::PaymentWorkflow{};

            if (false == LoadWorkflow(nym, id, out)) {
                throw std::runtime_error{
                    UnallocatedCString{"Workflow "} +
                    id.asBase58(api_.Crypto()) + " not found"};
            }

            return out;
        }();

        if (false == ContainsCheque(workflow)) {

            throw std::runtime_error{
                UnallocatedCString{"Workflow "} + id.asBase58(api_.Crypto()) +
                " does not contain a cheque"};
        }

        return session::Workflow::InstantiateCheque(api_, workflow);
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return {};
    }
}

auto Workflow::InstantiatePurse(
    const identifier::Nym& nym,
    const identifier::Generic& id) const -> Purse
{
    try {
        const auto workflow = [&] {
            auto out = protobuf::PaymentWorkflow{};

            if (false == LoadWorkflow(nym, id, out)) {
                throw std::runtime_error{
                    UnallocatedCString{"Workflow "} +
                    id.asBase58(api_.Crypto()) + " not found"};
            }

            return out;
        }();

        return session::Workflow::InstantiatePurse(api_, workflow);
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return {};
    }
}

auto Workflow::isCheque(const opentxs::Cheque& cheque) -> bool
{
    if (cheque.HasRemitter()) {
        LogError()()("Provided instrument is a voucher").Flush();

        return false;
    }

    if (0 > cheque.GetAmount()) {
        LogError()()("Provided instrument is an invoice").Flush();

        return false;
    }

    if (0 == cheque.GetAmount()) {
        LogError()()("Provided instrument is a cancellation").Flush();

        return false;
    }

    return true;
}

auto Workflow::isInternalTransfer(
    const identifier::Account& sourceAccount,
    const identifier::Account& destinationAccount) const -> bool
{
    const auto ownerNymID =
        api_.Storage().Internal().AccountOwner(sourceAccount);

    assert_false(ownerNymID.empty());

    const auto recipientNymID =
        api_.Storage().Internal().AccountOwner(destinationAccount);

    if (recipientNymID.empty()) { return false; }

    return ownerNymID == recipientNymID;
}

auto Workflow::isTransfer(const Item& item) -> bool
{
    return otx::itemType::transfer == item.GetType();
}

auto Workflow::List(
    const identifier::Nym& nymID,
    const PaymentWorkflowType type,
    const PaymentWorkflowState state) const
    -> UnallocatedSet<identifier::Generic>
{
    return api_.Storage().Internal().PaymentWorkflowsByState(
        nymID, type, state);
}

auto Workflow::LoadCheque(
    const identifier::Nym& nymID,
    const identifier::Generic& chequeID) const -> Workflow::Cheque
{
    auto workflow = get_workflow_by_source(
        {PaymentWorkflowType::OutgoingCheque,
         PaymentWorkflowType::IncomingCheque},
        nymID,
        chequeID);

    if (false == bool(workflow)) {
        LogError()()("Workflow for this cheque does not exist.").Flush();

        return {};
    }

    return session::Workflow::InstantiateCheque(api_, *workflow);
}

auto Workflow::LoadChequeByWorkflow(
    const identifier::Nym& nymID,
    const identifier::Generic& workflowID) const -> Workflow::Cheque
{
    auto workflow = get_workflow_by_id(
        {PaymentWorkflowType::OutgoingCheque,
         PaymentWorkflowType::IncomingCheque},
        nymID,
        workflowID);

    if (false == bool(workflow)) {
        LogError()()("Workflow for this cheque does not exist.").Flush();

        return {};
    }

    return session::Workflow::InstantiateCheque(api_, *workflow);
}

auto Workflow::LoadTransfer(
    const identifier::Nym& nymID,
    const identifier::Generic& transferID) const -> Workflow::Transfer
{
    auto workflow = get_workflow_by_source(
        {PaymentWorkflowType::OutgoingTransfer,
         PaymentWorkflowType::IncomingTransfer,
         PaymentWorkflowType::InternalTransfer},
        nymID,
        transferID);

    if (false == bool(workflow)) {
        LogError()()("Workflow for this transfer does not exist.").Flush();

        return {};
    }

    return InstantiateTransfer(api_, *workflow);
}

auto Workflow::LoadTransferByWorkflow(
    const identifier::Nym& nymID,
    const identifier::Generic& workflowID) const -> Workflow::Transfer
{
    auto workflow = get_workflow_by_id(
        {PaymentWorkflowType::OutgoingTransfer,
         PaymentWorkflowType::IncomingTransfer,
         PaymentWorkflowType::InternalTransfer},
        nymID,
        workflowID);

    if (false == bool(workflow)) {
        LogError()()("Workflow for this transfer does not exist.").Flush();

        return {};
    }

    return InstantiateTransfer(api_, *workflow);
}

auto Workflow::LoadWorkflow(
    const identifier::Nym& nymID,
    const identifier::Generic& workflowID,
    protobuf::PaymentWorkflow& out) const -> bool
{
    auto pWorkflow = get_workflow_by_id(nymID, workflowID);

    if (pWorkflow) {
        out = *pWorkflow;

        return true;
    } else {

        return false;
    }
}

auto Workflow::ReceiveCash(
    const identifier::Nym& receiver,
    const otx::blind::Purse& purse,
    const Message& message) const -> identifier::Generic
{
    const auto global = Lock{lock_};
    const auto serialized = String::Factory(message);
    const auto* party = message.nym_id_->Get();
    auto workflowID = api_.Factory().IdentifierFromRandom();
    protobuf::PaymentWorkflow workflow{};
    workflow.set_version(
        versions_.at(PaymentWorkflowType::IncomingCash).workflow_);
    workflow.set_id(workflowID.asBase58(api_.Crypto()));
    workflow.set_type(translate(PaymentWorkflowType::IncomingCash));
    workflow.set_state(translate(PaymentWorkflowState::Conveyed));
    auto& source = *(workflow.add_source());
    source.set_version(versions_.at(PaymentWorkflowType::IncomingCash).source_);
    source.set_id(workflowID.asBase58(api_.Crypto()));
    source.set_revision(1);
    source.set_item([&] {
        auto proto = protobuf::Purse{};
        purse.Internal().Serialize(proto);

        return to_string(proto);
    }());
    workflow.set_notary(purse.Notary().asBase58(api_.Crypto()));
    auto& event = *workflow.add_event();
    event.set_version(versions_.at(PaymentWorkflowType::IncomingCash).event_);
    event.set_time(message.time_);
    event.set_type(protobuf::PAYMENTEVENTTYPE_CONVEY);
    event.set_method(protobuf::TRANSPORTMETHOD_OT);
    event.set_transport(message.notary_id_->Get());
    event.add_item(serialized->Get());
    event.set_nym(party);
    event.set_success(true);
    workflow.add_unit(purse.Unit().asBase58(api_.Crypto()));
    workflow.add_party(party);
    const auto saved = save_workflow(receiver, workflow);

    if (false == saved) {
        LogError()()("Failed to save workflow").Flush();

        return {};
    }

    return workflowID;
}

auto Workflow::ReceiveCheque(
    const identifier::Nym& nymID,
    const opentxs::Cheque& cheque,
    const Message& message) const -> identifier::Generic
{
    if (false == isCheque(cheque)) { return {}; }

    if (false == validate_recipient(nymID, cheque)) {
        LogError()()("Nym ")(nymID, api_.Crypto())(
            " can not deposit this cheque.")
            .Flush();

        return {};
    }

    const auto global = Lock{lock_};
    const auto existing = get_workflow(
        global, {PaymentWorkflowType::IncomingCheque}, nymID, cheque);

    if (existing) {
        LogError()()("Workflow for this cheque already exist.").Flush();

        return api_.Factory().IdentifierFromBase58(existing->id());
    }

    const auto& party = cheque.GetSenderNymID();
    static const auto accountID = identifier::Account{};
    const auto [workflowID, workflow] = create_cheque(
        global,
        nymID,
        cheque,
        PaymentWorkflowType::IncomingCheque,
        PaymentWorkflowState::Conveyed,
        versions_.at(PaymentWorkflowType::IncomingCheque).workflow_,
        versions_.at(PaymentWorkflowType::IncomingCheque).source_,
        versions_.at(PaymentWorkflowType::IncomingCheque).event_,
        party,
        accountID,
        &message);

    if (false == workflowID.empty()) {
        const auto time = extract_conveyed_time(workflow);
        update_activity(
            nymID,
            cheque.GetSenderNymID(),
            api_.Factory().Internal().Identifier(cheque),
            workflowID,
            otx::client::StorageBox::INCOMINGCHEQUE,
            time);
        update_rpc(
            nymID,
            cheque.GetSenderNymID(),
            {},
            protobuf::ACCOUNTEVENT_INCOMINGCHEQUE,
            workflowID,
            0,
            cheque.GetAmount(),
            time,
            cheque.GetMemo().Get());
    }

    return workflowID;
}

auto Workflow::save_workflow(
    const identifier::Nym& nymID,
    const protobuf::PaymentWorkflow& workflow) const -> bool
{
    static const auto id = identifier::Account{};

    return save_workflow(nymID, id, workflow);
}

auto Workflow::save_workflow(
    const identifier::Nym& nymID,
    const identifier::Account& accountID,
    const protobuf::PaymentWorkflow& workflow) const -> bool
{
    const auto valid = protobuf::syntax::check(LogError(), workflow);

    assert_true(valid);

    const auto saved = api_.Storage().Internal().Store(nymID, workflow);

    assert_true(saved);

    if (false == accountID.empty()) {
        account_publisher_->Send([&] {
            auto work = opentxs::network::zeromq::tagged_message(
                WorkType::WorkflowAccountUpdate, true);
            accountID.Serialize(work);

            return work;
        }());
    }

    return valid && saved;
}

auto Workflow::save_workflow(
    identifier::Generic&& output,
    const identifier::Nym& nymID,
    const identifier::Account& accountID,
    const protobuf::PaymentWorkflow& workflow) const -> identifier::Generic
{
    if (save_workflow(nymID, accountID, workflow)) { return std::move(output); }

    return {};
}

auto Workflow::save_workflow(
    std::pair<identifier::Generic, protobuf::PaymentWorkflow>&& output,
    const identifier::Nym& nymID,
    const identifier::Account& accountID,
    const protobuf::PaymentWorkflow& workflow) const
    -> std::pair<identifier::Generic, protobuf::PaymentWorkflow>
{
    if (save_workflow(nymID, accountID, workflow)) { return std::move(output); }

    return {};
}

auto Workflow::SendCash(
    const identifier::Nym& sender,
    const identifier::Nym& recipient,
    const identifier::Generic& workflowID,
    const Message& request,
    const Message* reply) const -> bool
{
    auto global = Lock{lock_};
    const auto pWorkflow = get_workflow_by_id(sender, workflowID);

    if (false == bool(pWorkflow)) {
        LogError()()("Workflow ")(workflowID, api_.Crypto())(" does not exist.")
            .Flush();

        return false;
    }

    auto& workflow = *pWorkflow;
    auto lock = get_workflow_lock(global, workflowID.asBase58(api_.Crypto()));

    if (false == can_convey_cash(workflow)) { return false; }

    const bool haveReply = (nullptr != reply);

    if (haveReply) {
        workflow.set_state(translate(PaymentWorkflowState::Conveyed));
    }

    auto& event = *(workflow.add_event());
    event.set_version(versions_.at(PaymentWorkflowType::OutgoingCash).event_);
    event.set_type(protobuf::PAYMENTEVENTTYPE_CONVEY);
    event.add_item(String::Factory(request)->Get());
    event.set_method(protobuf::TRANSPORTMETHOD_OT);
    event.set_transport(request.notary_id_->Get());
    event.set_nym(request.nym_id2_->Get());

    if (haveReply) {
        event.add_item(String::Factory(*reply)->Get());
        event.set_time(reply->time_);
        event.set_success(reply->success_);
    } else {
        event.set_time(request.time_);
        event.set_success(false);
    }

    if (0 == workflow.party_size()) {
        workflow.add_party(recipient.asBase58(api_.Crypto()));
    }

    return save_workflow(sender, workflow);
}

auto Workflow::SendCheque(
    const opentxs::Cheque& cheque,
    const Message& request,
    const Message* reply) const -> bool
{
    if (false == isCheque(cheque)) { return false; }

    const auto& nymID = cheque.GetSenderNymID();
    auto global = Lock{lock_};
    const auto workflow = get_workflow(
        global, {PaymentWorkflowType::OutgoingCheque}, nymID, cheque);

    if (false == bool(workflow)) {
        LogError()()("Workflow for this cheque does not exist.").Flush();

        return false;
    }

    auto lock = get_workflow_lock(global, workflow->id());

    if (false == can_convey_cheque(*workflow)) { return false; }

    static const auto accountID = identifier::Account{};

    return add_cheque_event(
        lock,
        nymID,
        api_.Factory().NymIDFromBase58(request.nym_id2_->Bytes()),
        *workflow,
        PaymentWorkflowState::Conveyed,
        protobuf::PAYMENTEVENTTYPE_CONVEY,
        versions_.at(PaymentWorkflowType::OutgoingCheque).event_,
        request,
        reply,
        accountID);
}

auto Workflow::WorkflowParty(
    const identifier::Nym& nymID,
    const identifier::Generic& workflowID,
    const int index) const -> const UnallocatedCString
{
    auto workflow = get_workflow_by_id(nymID, workflowID);

    if (false == bool{workflow}) { return {}; }

    return workflow->party(index);
}

auto Workflow::WorkflowPartySize(
    const identifier::Nym& nymID,
    const identifier::Generic& workflowID,
    int& partysize) const -> bool
{
    auto workflow = get_workflow_by_id(nymID, workflowID);

    if (false == bool{workflow}) { return false; }

    partysize = workflow->party_size();

    return true;
}

auto Workflow::WorkflowState(
    const identifier::Nym& nymID,
    const identifier::Generic& workflowID) const -> PaymentWorkflowState
{
    auto workflow = get_workflow_by_id(nymID, workflowID);

    if (false == bool{workflow}) { return PaymentWorkflowState::Error; }

    return translate(workflow->state());
}

auto Workflow::WorkflowType(
    const identifier::Nym& nymID,
    const identifier::Generic& workflowID) const -> PaymentWorkflowType
{
    auto workflow = get_workflow_by_id(nymID, workflowID);

    if (false == bool{workflow}) { return PaymentWorkflowType::Error; }

    return translate(workflow->type());
}

auto Workflow::update_activity(
    const identifier::Nym& localNymID,
    const identifier::Nym& remoteNymID,
    const identifier::Generic& sourceID,
    const identifier::Generic& workflowID,
    const otx::client::StorageBox type,
    Time time) const -> bool
{
    const auto contactID = contact_.ContactID(remoteNymID);

    if (contactID.empty()) {
        LogError()()("Contact for nym ")(remoteNymID, api_.Crypto())(
            " does not exist")
            .Flush();

        return false;
    }

    const bool added = activity_.AddPaymentEvent(
        localNymID, contactID, type, sourceID, workflowID, time);

    if (added) {
        LogDetail()()("Success adding payment event to thread ")(
            contactID.asBase58(api_.Crypto()))
            .Flush();

        return true;
    } else {
        LogError()()("Failed to add payment event to thread ")(
            contactID.asBase58(api_.Crypto()))
            .Flush();

        return false;
    }
}

void Workflow::update_rpc(
    const identifier::Nym& localNymID,
    const identifier::Nym& remoteNymID,
    const UnallocatedCString& accountID,
    const protobuf::AccountEventType type,
    const identifier::Generic& workflowID,
    const Amount amount,
    const Amount pending,
    const Time time,
    const UnallocatedCString& memo) const
{
    protobuf::RPCPush push{};
    push.set_version(RPC_PUSH_VERSION);
    push.set_type(protobuf::RPCPUSH_ACCOUNT);
    push.set_id(localNymID.asBase58(api_.Crypto()));
    auto& event = *push.mutable_accountevent();
    event.set_version(RPC_ACCOUNT_EVENT_VERSION);
    event.set_id(accountID);
    event.set_type(type);

    if (false == remoteNymID.empty()) {
        event.set_contact(
            contact_.NymToContact(remoteNymID).asBase58(api_.Crypto()));
    }

    event.set_workflow(workflowID.asBase58(api_.Crypto()));
    amount.Serialize(writer(event.mutable_amount()));
    pending.Serialize(writer(event.mutable_pendingamount()));
    event.set_timestamp(seconds_since_epoch(time).value());
    event.set_memo(memo);

    assert_true(protobuf::syntax::check(LogError(), push));

    auto message = zmq::Message{};
    message.StartBody();
    message.AddFrame(localNymID);
    message.Internal().AddFrame(push);
    message.AddFrame(api_.Instance());
    rpc_publisher_->Send(std::move(message));
}

auto Workflow::validate_recipient(
    const identifier::Nym& nymID,
    const opentxs::Cheque& cheque) -> bool
{
    if (nymID.empty()) { return true; }

    return (nymID == cheque.GetRecipientNymID());
}

auto Workflow::WorkflowsByAccount(
    const identifier::Nym& nymID,
    const identifier::Account& accountID) const
    -> UnallocatedVector<identifier::Generic>
{
    UnallocatedVector<identifier::Generic> output{};
    const auto workflows =
        api_.Storage().Internal().PaymentWorkflowsByAccount(nymID, accountID);
    output.reserve(workflows.size());
    std::ranges::copy(workflows, std::back_inserter(output));

    return output;
}

auto Workflow::WriteCheque(const opentxs::Cheque& cheque) const
    -> identifier::Generic
{
    if (false == isCheque(cheque)) {
        LogError()()("Invalid item type on cheque object").Flush();

        return {};
    }

    const auto& nymID = cheque.GetSenderNymID();
    auto global = Lock{lock_};
    const auto existing = get_workflow(
        global, {PaymentWorkflowType::OutgoingCheque}, nymID, cheque);

    if (existing) {
        LogError()()("Workflow for this cheque already exist.").Flush();

        return api_.Factory().IdentifierFromBase58(existing->id());
    }

    if (cheque.HasRecipient()) {
        const auto& recipient = cheque.GetRecipientNymID();
        const auto contactID = contact_.ContactID(recipient);

        if (contactID.empty()) {
            LogError()()("No contact exists for recipient nym ")(
                recipient, api_.Crypto())
                .Flush();

            return {};
        }
    }

    const auto party =
        cheque.HasRecipient() ? cheque.GetRecipientNymID() : identifier::Nym{};
    const auto [workflowID, workflow] = create_cheque(
        global,
        nymID,
        cheque,
        PaymentWorkflowType::OutgoingCheque,
        PaymentWorkflowState::Unsent,
        versions_.at(PaymentWorkflowType::OutgoingCheque).workflow_,
        versions_.at(PaymentWorkflowType::OutgoingCheque).source_,
        versions_.at(PaymentWorkflowType::OutgoingCheque).event_,
        party,
        cheque.GetSenderAcctID());
    global.unlock();
    const bool haveWorkflow = (false == workflowID.empty());
    const auto time{
        seconds_since_epoch_unsigned(workflow.event(0).time()).value()};

    if (haveWorkflow && cheque.HasRecipient()) {
        update_activity(
            cheque.GetSenderNymID(),
            cheque.GetRecipientNymID(),
            api_.Factory().Internal().Identifier(cheque),
            workflowID,
            otx::client::StorageBox::OUTGOINGCHEQUE,
            time);
    }

    if (false == workflowID.empty()) {
        update_rpc(
            nymID,
            cheque.GetRecipientNymID(),
            cheque.SourceAccountID().asBase58(api_.Crypto()),
            protobuf::ACCOUNTEVENT_OUTGOINGCHEQUE,
            workflowID,
            0,
            -1 * cheque.GetAmount(),
            time,
            cheque.GetMemo().Get());
    }

    return workflowID;
}
}  // namespace opentxs::api::session::imp
