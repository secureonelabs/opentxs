// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "interface/ui/accountactivity/CustodialAccountActivity.hpp"  // IWYU pragma: associated

#include <opentxs/protobuf/PaymentEvent.pb.h>
#include <opentxs/protobuf/PaymentWorkflow.pb.h>
#include <opentxs/protobuf/PaymentWorkflowEnums.pb.h>
#include <atomic>
#include <chrono>
#include <compare>
#include <future>
#include <memory>
#include <optional>
#include <span>

#include "internal/api/session/Storage.hpp"
#include "internal/api/session/Types.hpp"
#include "internal/core/Factory.hpp"
#include "internal/core/contract/ServerContract.hpp"
#include "internal/core/contract/Unit.hpp"
#include "internal/otx/common/Account.hpp"
#include "internal/util/Mutex.hpp"
#include "opentxs/AccountType.hpp"  // IWYU pragma: keep
#include "opentxs/Time.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/api/session/Client.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Endpoints.hpp"  // IWYU pragma: keep
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Storage.hpp"
#include "opentxs/api/session/Wallet.hpp"
#include "opentxs/api/session/Wallet.internal.hpp"
#include "opentxs/api/session/Workflow.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/display/Definition.hpp"
#include "opentxs/identifier/Generic.hpp"
#include "opentxs/identifier/Notary.hpp"
#include "opentxs/identifier/Types.hpp"
#include "opentxs/identifier/UnitDefinition.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/otx/client/PaymentWorkflowState.hpp"  // IWYU pragma: keep
#include "opentxs/otx/client/PaymentWorkflowType.hpp"   // IWYU pragma: keep
#include "opentxs/otx/client/Types.hpp"
#include "opentxs/protobuf/Types.internal.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Writer.hpp"

namespace opentxs::factory
{
auto CustodialAccountActivityModel(
    const api::session::Client& api,
    const identifier::Nym& nymID,
    const identifier::Account& accountID,
    const SimpleCallback& cb) noexcept
    -> std::unique_ptr<ui::internal::AccountActivity>
{
    using ReturnType = ui::implementation::CustodialAccountActivity;

    if (AccountType::Custodial != accountID.AccountType()) {
        LogAbort()()("wrong identifier type for ")(accountID.asHex())(": ")(
            print(accountID.Subtype()))
            .Abort();
    }

    return std::make_unique<ReturnType>(api, nymID, accountID, cb);
}
}  // namespace opentxs::factory

namespace opentxs::ui::implementation
{
CustodialAccountActivity::CustodialAccountActivity(
    const api::session::Client& api,
    const identifier::Nym& nymID,
    const identifier::Account& accountID,
    const SimpleCallback& cb) noexcept
    : AccountActivity(api, nymID, accountID, AccountType::Custodial, cb)
    , alias_()
{
    init({
        UnallocatedCString{api.Endpoints().AccountUpdate()},
        UnallocatedCString{api.Endpoints().ContactUpdate()},
        UnallocatedCString{api.Endpoints().ServerUpdate()},
        UnallocatedCString{api.Endpoints().UnitUpdate()},
        UnallocatedCString{api.Endpoints().WorkflowAccountUpdate()},
    });

    // If an Account exists, then the unit definition and notary contracts must
    // exist already.
    assert_true(0 < Contract().Version());
    assert_true(0 < Notary().Version());
}

auto CustodialAccountActivity::ContractID() const noexcept -> UnallocatedCString
{
    auto lock = sLock{shared_lock_};

    return contract_->ID().asBase58(api_.Crypto());
}

auto CustodialAccountActivity::display_balance(
    opentxs::Amount amount) const noexcept -> UnallocatedCString
{
    auto lock = sLock{shared_lock_};
    const auto& definition = display::GetDefinition(Contract().UnitOfAccount());
    UnallocatedCString output = definition.Format(amount);

    if (0 < output.size()) { return output; }

    amount.Serialize(writer(output));
    return output;
}

auto CustodialAccountActivity::DisplayUnit() const noexcept
    -> UnallocatedCString
{
    auto lock = sLock{shared_lock_};
    const auto& definition = display::GetDefinition(Unit());
    return UnallocatedCString{definition.ShortName()};
}

auto CustodialAccountActivity::extract_event(
    const protobuf::PaymentEventType eventType,
    const protobuf::PaymentWorkflow& workflow) noexcept -> EventRow
{
    bool success{false};
    bool found{false};
    EventRow output{};
    auto& [time, event_p] = output;

    for (const auto& event : workflow.event()) {
        const auto eventTime =
            seconds_since_epoch_unsigned(event.time()).value();

        if (eventType != event.type()) { continue; }

        if (eventTime > time) {
            if (success) {
                if (event.success()) {
                    time = eventTime;
                    event_p = &event;
                    found = true;
                }
            } else {
                time = eventTime;
                event_p = &event;
                success = event.success();
                found = true;
            }
        } else {
            if (false == success) {
                if (event.success()) {
                    // This is a weird case. It probably shouldn't happen
                    time = eventTime;
                    event_p = &event;
                    success = true;
                    found = true;
                }
            }
        }
    }

    if (false == found) {
        LogError()()("Workflow ")(workflow.id())(", type ")(workflow.type())(
            ", state ")(workflow.state())(
            " does not contain an event of type ")(eventType)
            .Flush();

        LogAbort()().Abort();
    }

    return output;
}

auto CustodialAccountActivity::extract_rows(
    const protobuf::PaymentWorkflow& workflow) noexcept
    -> UnallocatedVector<RowKey>
{
    auto output = UnallocatedVector<RowKey>{};

    switch (translate(workflow.type())) {
        case otx::client::PaymentWorkflowType::OutgoingCheque: {
            switch (translate(workflow.state())) {
                case otx::client::PaymentWorkflowState::Unsent:
                case otx::client::PaymentWorkflowState::Conveyed:
                case otx::client::PaymentWorkflowState::Expired: {
                    output.emplace_back(
                        protobuf::PAYMENTEVENTTYPE_CREATE,
                        extract_event(
                            protobuf::PAYMENTEVENTTYPE_CREATE, workflow));
                } break;
                case otx::client::PaymentWorkflowState::Cancelled: {
                    output.emplace_back(
                        protobuf::PAYMENTEVENTTYPE_CREATE,
                        extract_event(
                            protobuf::PAYMENTEVENTTYPE_CREATE, workflow));
                    output.emplace_back(
                        protobuf::PAYMENTEVENTTYPE_CANCEL,
                        extract_event(
                            protobuf::PAYMENTEVENTTYPE_CANCEL, workflow));
                } break;
                case otx::client::PaymentWorkflowState::Accepted:
                case otx::client::PaymentWorkflowState::Completed: {
                    output.emplace_back(
                        protobuf::PAYMENTEVENTTYPE_CREATE,
                        extract_event(
                            protobuf::PAYMENTEVENTTYPE_CREATE, workflow));
                    output.emplace_back(
                        protobuf::PAYMENTEVENTTYPE_ACCEPT,
                        extract_event(
                            protobuf::PAYMENTEVENTTYPE_ACCEPT, workflow));
                } break;
                case otx::client::PaymentWorkflowState::Error:
                case otx::client::PaymentWorkflowState::Initiated:
                case otx::client::PaymentWorkflowState::Aborted:
                case otx::client::PaymentWorkflowState::Acknowledged:
                case otx::client::PaymentWorkflowState::Rejected:
                default: {
                    LogError()()("Invalid workflow state (")(workflow.state())(
                        ")")
                        .Flush();
                }
            }
        } break;
        case otx::client::PaymentWorkflowType::IncomingCheque: {
            switch (translate(workflow.state())) {
                case otx::client::PaymentWorkflowState::Conveyed:
                case otx::client::PaymentWorkflowState::Expired:
                case otx::client::PaymentWorkflowState::Completed: {
                    output.emplace_back(
                        protobuf::PAYMENTEVENTTYPE_CONVEY,
                        extract_event(
                            protobuf::PAYMENTEVENTTYPE_CONVEY, workflow));
                } break;
                case otx::client::PaymentWorkflowState::Error:
                case otx::client::PaymentWorkflowState::Unsent:
                case otx::client::PaymentWorkflowState::Cancelled:
                case otx::client::PaymentWorkflowState::Accepted:
                case otx::client::PaymentWorkflowState::Initiated:
                case otx::client::PaymentWorkflowState::Aborted:
                case otx::client::PaymentWorkflowState::Acknowledged:
                case otx::client::PaymentWorkflowState::Rejected:
                default: {
                    LogError()()("Invalid workflow state (")(workflow.state())(
                        ")")
                        .Flush();
                }
            }
        } break;
        case otx::client::PaymentWorkflowType::OutgoingTransfer: {
            switch (translate(workflow.state())) {
                case otx::client::PaymentWorkflowState::Acknowledged:
                case otx::client::PaymentWorkflowState::Accepted: {
                    output.emplace_back(
                        protobuf::PAYMENTEVENTTYPE_ACKNOWLEDGE,
                        extract_event(
                            protobuf::PAYMENTEVENTTYPE_ACKNOWLEDGE, workflow));
                } break;
                case otx::client::PaymentWorkflowState::Completed: {
                    output.emplace_back(
                        protobuf::PAYMENTEVENTTYPE_ACKNOWLEDGE,
                        extract_event(
                            protobuf::PAYMENTEVENTTYPE_ACKNOWLEDGE, workflow));
                    output.emplace_back(
                        protobuf::PAYMENTEVENTTYPE_COMPLETE,
                        extract_event(
                            protobuf::PAYMENTEVENTTYPE_COMPLETE, workflow));
                } break;
                case otx::client::PaymentWorkflowState::Initiated:
                case otx::client::PaymentWorkflowState::Aborted: {
                } break;
                case otx::client::PaymentWorkflowState::Error:
                case otx::client::PaymentWorkflowState::Unsent:
                case otx::client::PaymentWorkflowState::Conveyed:
                case otx::client::PaymentWorkflowState::Cancelled:
                case otx::client::PaymentWorkflowState::Expired:
                case otx::client::PaymentWorkflowState::Rejected:
                default: {
                    LogError()()("Invalid workflow state (")(workflow.state())(
                        ")")
                        .Flush();
                }
            }
        } break;
        case otx::client::PaymentWorkflowType::IncomingTransfer: {
            switch (translate(workflow.state())) {
                case otx::client::PaymentWorkflowState::Conveyed: {
                    output.emplace_back(
                        protobuf::PAYMENTEVENTTYPE_CONVEY,
                        extract_event(
                            protobuf::PAYMENTEVENTTYPE_CONVEY, workflow));
                } break;
                case otx::client::PaymentWorkflowState::Completed: {
                    output.emplace_back(
                        protobuf::PAYMENTEVENTTYPE_CONVEY,
                        extract_event(
                            protobuf::PAYMENTEVENTTYPE_CONVEY, workflow));
                    output.emplace_back(
                        protobuf::PAYMENTEVENTTYPE_ACCEPT,
                        extract_event(
                            protobuf::PAYMENTEVENTTYPE_ACCEPT, workflow));
                } break;
                case otx::client::PaymentWorkflowState::Error:
                case otx::client::PaymentWorkflowState::Unsent:
                case otx::client::PaymentWorkflowState::Cancelled:
                case otx::client::PaymentWorkflowState::Accepted:
                case otx::client::PaymentWorkflowState::Expired:
                case otx::client::PaymentWorkflowState::Initiated:
                case otx::client::PaymentWorkflowState::Aborted:
                case otx::client::PaymentWorkflowState::Acknowledged:
                case otx::client::PaymentWorkflowState::Rejected:
                default: {
                    LogError()()("Invalid workflow state (")(workflow.state())(
                        ")")
                        .Flush();
                }
            }
        } break;
        case otx::client::PaymentWorkflowType::InternalTransfer: {
            switch (translate(workflow.state())) {
                case otx::client::PaymentWorkflowState::Acknowledged:
                case otx::client::PaymentWorkflowState::Conveyed:
                case otx::client::PaymentWorkflowState::Accepted: {
                    output.emplace_back(
                        protobuf::PAYMENTEVENTTYPE_ACKNOWLEDGE,
                        extract_event(
                            protobuf::PAYMENTEVENTTYPE_ACKNOWLEDGE, workflow));
                } break;
                case otx::client::PaymentWorkflowState::Completed: {
                    output.emplace_back(
                        protobuf::PAYMENTEVENTTYPE_ACKNOWLEDGE,
                        extract_event(
                            protobuf::PAYMENTEVENTTYPE_ACKNOWLEDGE, workflow));
                    output.emplace_back(
                        protobuf::PAYMENTEVENTTYPE_COMPLETE,
                        extract_event(
                            protobuf::PAYMENTEVENTTYPE_COMPLETE, workflow));
                } break;
                case otx::client::PaymentWorkflowState::Initiated:
                case otx::client::PaymentWorkflowState::Aborted: {
                } break;
                case otx::client::PaymentWorkflowState::Error:
                case otx::client::PaymentWorkflowState::Unsent:
                case otx::client::PaymentWorkflowState::Cancelled:
                case otx::client::PaymentWorkflowState::Expired:
                case otx::client::PaymentWorkflowState::Rejected:
                default: {
                    LogError()()("Invalid workflow state (")(workflow.state())(
                        ")")
                        .Flush();
                }
            }
        } break;
        case otx::client::PaymentWorkflowType::Error:
        case otx::client::PaymentWorkflowType::OutgoingInvoice:
        case otx::client::PaymentWorkflowType::IncomingInvoice:
        case otx::client::PaymentWorkflowType::OutgoingCash:
        case otx::client::PaymentWorkflowType::IncomingCash:
        default: {
            LogError()()("Unsupported workflow type (")(workflow.type())(")")
                .Flush();
        }
    }

    return output;
}

auto CustodialAccountActivity::Name() const noexcept -> UnallocatedCString
{
    const auto& api = api_;

    return account_name_custodial(
        api,
        api.Storage().Internal().AccountServer(account_id_),
        api.Storage().Internal().AccountContract(account_id_),
        [this] {
            auto lock = sLock{shared_lock_};

            return alias_;
        }());
}

auto CustodialAccountActivity::NotaryID() const noexcept -> UnallocatedCString
{
    auto lock = sLock{shared_lock_};

    return notary_->ID().asBase58(api_.Crypto());
}

auto CustodialAccountActivity::NotaryName() const noexcept -> UnallocatedCString
{
    auto lock = sLock{shared_lock_};

    return notary_->EffectiveName();
}

auto CustodialAccountActivity::pipeline(const Message& in) noexcept -> void
{
    if (false == running_.load()) { return; }

    const auto body = in.Payload();

    if (1 > body.size()) {
        LogError()()("Invalid message").Flush();

        LogAbort()().Abort();
    }

    const auto work = [&] {
        try {

            return body[0].as<Work>();
        } catch (...) {

            LogAbort()().Abort();
        }
    }();

    switch (work) {
        case Work::notary: {
            process_notary(in);
        } break;
        case Work::unit: {
            process_unit(in);
        } break;
        case Work::contact: {
            process_contact(in);
        } break;
        case Work::account: {
            process_balance(in);
        } break;
        case Work::workflow: {
            process_workflow(in);
        } break;
        case Work::init: {
            startup();
            finish_startup();
        } break;
        case Work::statemachine: {
            do_work();
        } break;
        case Work::shutdown: {
            if (auto previous = running_.exchange(false); previous) {
                shutdown(shutdown_promise_);
            }
        } break;
        default: {
            LogError()()("Unhandled type").Flush();

            LogAbort()().Abort();
        }
    }
}

auto CustodialAccountActivity::process_balance(const Message& message) noexcept
    -> void
{
    wait_for_startup();
    const auto body = message.Payload();

    assert_true(2 < body.size());

    const auto accountID = api_.Factory().AccountIDFromZMQ(body[1]);

    if (account_id_ != accountID) { return; }

    const auto balance = factory::Amount(body[2]);
    const auto oldBalance = [&] {
        auto lock = eLock{shared_lock_};

        const auto oldbalance = balance_;
        balance_ = balance;
        return oldbalance;
    }();
    const auto balanceChanged = (oldBalance != balance);
    const auto alias = [&] {
        auto account = api_.Wallet().Internal().Account(account_id_);

        assert_true(account);

        return account.get().Alias();
    }();
    const auto aliasChanged = [&] {
        auto lock = eLock{shared_lock_};

        if (alias != alias_) {
            alias_ = alias;

            return true;
        }

        return false;
    }();

    if (balanceChanged) { notify_balance(balance); }

    if (aliasChanged) { UpdateNotify(); }
}

auto CustodialAccountActivity::process_contact(const Message& message) noexcept
    -> void
{
    wait_for_startup();
    // Contact names may have changed, therefore all row texts must be
    // recalculated
    startup();
}

auto CustodialAccountActivity::process_notary(const Message& message) noexcept
    -> void
{
    wait_for_startup();
    const auto oldName = NotaryName();
    const auto newName = [&] {
        {
            auto lock = eLock{shared_lock_};
            notary_ = api_.Wallet().Internal().Server(
                api_.Storage().Internal().AccountServer(account_id_));
        }

        return NotaryName();
    }();

    if (oldName != newName) {
        // TODO Qt widgets need to know that notary name property has changed
        UpdateNotify();
    }
}

auto CustodialAccountActivity::process_workflow(
    const identifier::Generic& workflowID,
    UnallocatedSet<AccountActivityRowID>& active) noexcept -> void
{
    const auto workflow = [&] {
        auto out = protobuf::PaymentWorkflow{};
        api_.Workflow().LoadWorkflow(primary_id_, workflowID, out);

        return out;
    }();
    const auto rows = extract_rows(workflow);

    for (const auto& [type, row] : rows) {
        const auto& [time, event_p] = row;
        auto key = AccountActivityRowID{workflowID, type};
        auto custom = CustomData{
            new protobuf::PaymentWorkflow(workflow),
            new protobuf::PaymentEvent(*event_p)};
        add_item(key, time, custom);
        active.emplace(std::move(key));
    }
}

auto CustodialAccountActivity::process_workflow(const Message& message) noexcept
    -> void
{
    wait_for_startup();
    const auto body = message.Payload();

    assert_true(1 < body.size());

    const auto accountID = api_.Factory().AccountIDFromZMQ(body[1]);

    assert_false(accountID.empty());

    if (account_id_ == accountID) { startup(); }
}

auto CustodialAccountActivity::process_unit(const Message& message) noexcept
    -> void
{
    wait_for_startup();
    // TODO currently it doesn't matter if the unit definition alias changes
    // since we don't use it
    auto lock = eLock{shared_lock_};
    contract_ = api_.Wallet().Internal().UnitDefinition(
        api_.Storage().Internal().AccountContract(account_id_));
}

auto CustodialAccountActivity::startup() noexcept -> void
{
    const auto alias = [&] {
        auto account = api_.Wallet().Internal().Account(account_id_);

        assert_true(account);

        return account.get().Alias();
    }();
    const auto aliasChanged = [&] {
        auto lock = eLock{shared_lock_};

        if (alias != alias_) {
            alias_ = alias;

            return true;
        }

        return false;
    }();

    const auto workflows =
        api_.Workflow().WorkflowsByAccount(primary_id_, account_id_);
    auto active = UnallocatedSet<AccountActivityRowID>{};

    for (const auto& id : workflows) { process_workflow(id, active); }

    delete_inactive(active);

    if (aliasChanged) {
        // TODO Qt widgets need to know the alias property has changed
        UpdateNotify();
    }
}

auto CustodialAccountActivity::Unit() const noexcept -> UnitType
{
    auto lock = sLock{shared_lock_};

    return contract_->UnitOfAccount();
}

CustodialAccountActivity::~CustodialAccountActivity()
{
    wait_for_startup();
    signal_shutdown().get();
}
}  // namespace opentxs::ui::implementation
