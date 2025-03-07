// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <ratio>
#include <span>
#include <string_view>
#include <utility>

#include "internal/api/session/OTX.hpp"
#include "internal/network/zeromq/ListenCallback.hpp"
#include "internal/network/zeromq/socket/Publish.hpp"
#include "internal/network/zeromq/socket/Pull.hpp"
#include "internal/network/zeromq/socket/Subscribe.hpp"
#include "internal/otx/client/Client.hpp"
#include "internal/otx/client/OTPayment.hpp"
#include "internal/otx/common/Cheque.hpp"
#include "internal/util/Flag.hpp"
#include "internal/util/Lockable.hpp"
#include "internal/util/Mutex.hpp"
#include "internal/util/UniqueQueue.hpp"
#include "opentxs/Export.hpp"
#include "opentxs/Time.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/Types.internal.hpp"
#include "opentxs/core/Amount.hpp"
#include "opentxs/core/contract/peer/Types.hpp"
#include "opentxs/identifier/Account.hpp"
#include "opentxs/identifier/Generic.hpp"
#include "opentxs/identifier/Notary.hpp"
#include "opentxs/identifier/Nym.hpp"
#include "opentxs/identifier/UnitDefinition.hpp"
#include "opentxs/otx/LastReplyStatus.hpp"  // IWYU pragma: keep
#include "opentxs/otx/Types.hpp"
#include "opentxs/otx/client/Types.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/PasswordPrompt.hpp"
#include "otx/client/StateMachine.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs
{
namespace api
{
namespace session
{
class Client;
}  // namespace session
}  // namespace api

namespace blockchain
{
namespace block
{
class Transaction;
}  // namespace block
}  // namespace blockchain

namespace contract
{
class Server;
}  // namespace contract

namespace identity
{
namespace wot
{
class Claim;
class Verification;
}  // namespace wot
}  // namespace identity

namespace network
{
namespace zeromq
{
class Message;
}  // namespace zeromq
}  // namespace network

namespace protobuf
{
class ServerContract;
}  // namespace protobuf
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::api::session::imp
{
class OPENTXS_NO_EXPORT OTX final : virtual public internal::OTX, Lockable
{
public:
    static constexpr auto default_cheque_interval = std::chrono::hours{24 * 30};

    auto AcknowledgeBailment(
        const identifier::Nym& localNymID,
        const identifier::Nym& targetNymID,
        const identifier::Generic& requestID,
        std::string_view instructions,
        const otx::client::SetID setID) const -> BackgroundTask final;
    auto AcknowledgeBailmentNotice(
        const identifier::Nym& localNymID,
        const identifier::Nym& recipientID,
        const identifier::Generic& requestID,
        const bool ack,
        const otx::client::SetID setID) const -> BackgroundTask final;
    auto AcknowledgeConnection(
        const identifier::Nym& localNymID,
        const identifier::Nym& recipientID,
        const identifier::Generic& requestID,
        const bool ack,
        std::string_view url,
        std::string_view login,
        std::string_view password,
        std::string_view key,
        const otx::client::SetID setID) const -> BackgroundTask final;
    auto AcknowledgeFaucet(
        const identifier::Nym& localNymID,
        const identifier::Nym& targetNymID,
        const identifier::Generic& requestID,
        const blockchain::block::Transaction& transaction,
        const otx::client::SetID setID) const -> BackgroundTask final;
    auto AcknowledgeOutbailment(
        const identifier::Nym& localNymID,
        const identifier::Nym& recipientID,
        const identifier::Generic& requestID,
        std::string_view details,
        const otx::client::SetID setID) const -> BackgroundTask final;
    auto AcknowledgeStoreSecret(
        const identifier::Nym& localNymID,
        const identifier::Nym& recipientID,
        const identifier::Generic& requestID,
        const bool ack,
        const otx::client::SetID setID) const -> BackgroundTask final;
    auto AcknowledgeVerification(
        const identifier::Nym& localNymID,
        const identifier::Nym& recipientID,
        const identifier::Generic& requestID,
        std::optional<identity::wot::Verification> response,
        const otx::client::SetID setID = {}) const -> BackgroundTask final;
    auto AutoProcessInboxEnabled() const -> bool final
    {
        return auto_process_inbox_.get();
    }
    auto CanDeposit(
        const identifier::Nym& recipientNymID,
        const OTPayment& payment) const -> otx::client::Depositability final;
    auto CanDeposit(
        const identifier::Nym& recipientNymID,
        const identifier::Account& accountID,
        const OTPayment& payment) const -> otx::client::Depositability final;
    auto CanMessage(
        const identifier::Nym& senderNymID,
        const identifier::Generic& recipientContactID,
        const bool startIntroductionServer) const
        -> otx::client::Messagability final;
    auto CheckTransactionNumbers(
        const identifier::Nym& nym,
        const identifier::Notary& serverID,
        const std::size_t quantity) const -> bool final;
    auto ContextIdle(
        const identifier::Nym& nym,
        const identifier::Notary& server) const -> Finished final;
    auto DepositCheques(const identifier::Nym& nymID) const
        -> std::size_t final;
    auto DepositCheques(
        const identifier::Nym& nymID,
        const UnallocatedSet<identifier::Generic>& chequeIDs) const
        -> std::size_t final;
    auto DepositPayment(
        const identifier::Nym& recipientNymID,
        const std::shared_ptr<const OTPayment>& payment) const
        -> BackgroundTask final;
    auto DepositPayment(
        const identifier::Nym& recipientNymID,
        const identifier::Account& accountID,
        const std::shared_ptr<const OTPayment>& payment) const
        -> BackgroundTask final;
    void DisableAutoaccept() const final;
    auto DownloadMint(
        const identifier::Nym& nym,
        const identifier::Notary& server,
        const identifier::UnitDefinition& unit) const -> BackgroundTask final;
    auto DownloadNym(
        const identifier::Nym& localNymID,
        const identifier::Notary& serverID,
        const identifier::Nym& targetNymID) const -> BackgroundTask final;
    auto DownloadNymbox(
        const identifier::Nym& localNymID,
        const identifier::Notary& serverID) const -> BackgroundTask final;
    auto DownloadServerContract(
        const identifier::Nym& localNymID,
        const identifier::Notary& serverID,
        const identifier::Notary& contractID) const -> BackgroundTask final;
    auto DownloadUnitDefinition(
        const identifier::Nym& localNymID,
        const identifier::Notary& serverID,
        const identifier::UnitDefinition& contractID) const
        -> BackgroundTask final;
    auto FindNym(const identifier::Nym& nymID) const -> BackgroundTask final;
    auto FindNym(
        const identifier::Nym& nymID,
        const identifier::Notary& serverIDHint) const -> BackgroundTask final;
    auto FindServer(const identifier::Notary& serverID) const
        -> BackgroundTask final;
    auto FindUnitDefinition(const identifier::UnitDefinition& unit) const
        -> BackgroundTask final;
    auto InitiateBailment(
        const identifier::Nym& localNymID,
        const identifier::Notary& serverID,
        const identifier::Nym& targetNymID,
        const identifier::UnitDefinition& instrumentDefinitionID,
        const otx::client::SetID setID) const -> BackgroundTask final;
    auto InitiateFaucet(
        const identifier::Nym& localNymID,
        const identifier::Nym& targetNymID,
        opentxs::UnitType unit,
        std::string_view address,
        const otx::client::SetID setID) const -> BackgroundTask final;
    auto InitiateOutbailment(
        const identifier::Nym& localNymID,
        const identifier::Notary& serverID,
        const identifier::Nym& targetNymID,
        const identifier::UnitDefinition& instrumentDefinitionID,
        const Amount& amount,
        std::string_view message,
        const otx::client::SetID setID) const -> BackgroundTask final;
    auto InitiateRequestConnection(
        const identifier::Nym& localNymID,
        const identifier::Nym& targetNymID,
        const contract::peer::ConnectionInfoType& type,
        const otx::client::SetID setID) const -> BackgroundTask final;
    auto InitiateStoreSecret(
        const identifier::Nym& localNymID,
        const identifier::Nym& targetNymID,
        const contract::peer::SecretType& type,
        std::span<const std::string_view> data,
        const otx::client::SetID setID) const -> BackgroundTask final;
    auto InitiateVerification(
        const identifier::Nym& localNymID,
        const identifier::Nym& targetNymID,
        const identity::wot::Claim& claim,
        const otx::client::SetID setID = {}) const -> BackgroundTask final;
    auto IntroductionServer() const -> const identifier::Notary& final;
    auto IssueUnitDefinition(
        const identifier::Nym& localNymID,
        const identifier::Notary& serverID,
        const identifier::UnitDefinition& unitID,
        const UnitType advertise,
        const UnallocatedCString& label) const -> BackgroundTask final;
    auto MessageContact(
        const identifier::Nym& senderNymID,
        const identifier::Generic& contactID,
        const UnallocatedCString& message,
        const otx::client::SetID setID) const -> BackgroundTask final;
    auto MessageStatus(const TaskID taskID) const
        -> std::pair<otx::client::ThreadStatus, MessageID> final;
    auto NotifyBailment(
        const identifier::Nym& localNymID,
        const identifier::Notary& serverID,
        const identifier::Nym& targetNymID,
        const identifier::UnitDefinition& instrumentDefinitionID,
        const identifier::Generic& requestID,
        std::string_view txid,
        const Amount& amount,
        const otx::client::SetID setID) const -> BackgroundTask final;
    auto PayContact(
        const identifier::Nym& senderNymID,
        const identifier::Generic& contactID,
        std::shared_ptr<const OTPayment> payment) const -> BackgroundTask final;
    auto PayContactCash(
        const identifier::Nym& senderNymID,
        const identifier::Generic& contactID,
        const identifier::Generic& workflowID) const -> BackgroundTask final;
    auto ProcessInbox(
        const identifier::Nym& localNymID,
        const identifier::Notary& serverID,
        const identifier::Account& accountID) const -> BackgroundTask final;
    auto PublishServerContract(
        const identifier::Nym& localNymID,
        const identifier::Notary& serverID,
        const identifier::Generic& contractID) const -> BackgroundTask final;
    void Refresh() const final;
    auto RefreshCount() const -> std::uint64_t final;
    auto RegisterAccount(
        const identifier::Nym& localNymID,
        const identifier::Notary& serverID,
        const identifier::UnitDefinition& unitID,
        const UnallocatedCString& label) const -> BackgroundTask final;
    auto RegisterNym(
        const identifier::Nym& localNymID,
        const identifier::Notary& serverID,
        const bool resync) const -> BackgroundTask final;
    auto RegisterNymPublic(
        const identifier::Nym& nymID,
        const identifier::Notary& server,
        const bool setContactData,
        const bool forcePrimary,
        const bool resync) const -> BackgroundTask final;
    auto SetIntroductionServer(const contract::Server& contract) const noexcept
        -> identifier::Notary final;
    auto SetIntroductionServer(ReadView contract) const noexcept
        -> identifier::Notary final;
    auto SendCheque(
        const identifier::Nym& localNymID,
        const identifier::Account& sourceAccountID,
        const identifier::Generic& recipientContactID,
        const Amount& value,
        const UnallocatedCString& memo,
        const Time validFrom) const -> BackgroundTask final
    {
        return SendCheque(
            localNymID,
            sourceAccountID,
            recipientContactID,
            value,
            memo,
            validFrom,
            validFrom + default_cheque_interval);
    }
    auto SendCheque(
        const identifier::Nym& localNymID,
        const identifier::Account& sourceAccountID,
        const identifier::Generic& recipientContactID,
        const Amount& value,
        const UnallocatedCString& memo,
        const Time validFrom,
        const Time validTo) const -> BackgroundTask final;
    auto SendExternalTransfer(
        const identifier::Nym& localNymID,
        const identifier::Notary& serverID,
        const identifier::Account& sourceAccountID,
        const identifier::Account& targetAccountID,
        const Amount& value,
        const UnallocatedCString& memo) const -> BackgroundTask final;
    auto SendTransfer(
        const identifier::Nym& localNymID,
        const identifier::Notary& serverID,
        const identifier::Account& sourceAccountID,
        const identifier::Account& targetAccountID,
        const Amount& value,
        const UnallocatedCString& memo) const -> BackgroundTask final;
    void StartIntroductionServer(const identifier::Nym& localNymID) const final;
    auto Status(const TaskID taskID) const -> otx::client::ThreadStatus final;
    auto WithdrawCash(
        const identifier::Nym& nymID,
        const identifier::Notary& serverID,
        const identifier::Account& account,
        const Amount& value) const -> BackgroundTask final;

    OTX(const Flag& running,
        const api::session::Client& client,
        ContextLockCallback lockCallback);
    OTX() = delete;
    OTX(const OTX&) = delete;
    OTX(OTX&&) = delete;
    auto operator=(const OTX&) -> OTX& = delete;
    auto operator=(OTX&&) -> OTX& = delete;

    ~OTX() final;

private:
    using TaskStatusMap = UnallocatedMap<
        TaskID,
        std::pair<otx::client::ThreadStatus, std::promise<Result>>>;
    using ContextID = std::pair<identifier::Nym, identifier::Notary>;

    ContextLockCallback lock_callback_;
    const Flag& running_;
    const api::session::Client& api_;
    mutable std::mutex introduction_server_lock_{};
    mutable std::mutex nym_fetch_lock_{};
    mutable std::mutex task_status_lock_{};
    mutable std::atomic<std::uint64_t> refresh_counter_{0};
    mutable UnallocatedMap<ContextID, otx::client::implementation::StateMachine>
        operations_;
    mutable UnallocatedMap<identifier::Generic, UniqueQueue<identifier::Nym>>
        server_nym_fetch_;
    UniqueQueue<otx::client::CheckNymTask> missing_nyms_;
    UniqueQueue<otx::client::CheckNymTask> outdated_nyms_;
    UniqueQueue<identifier::Notary> missing_servers_;
    UniqueQueue<identifier::UnitDefinition> missing_unit_definitions_;
    mutable std::unique_ptr<identifier::Notary> introduction_server_id_;
    mutable TaskStatusMap task_status_;
    mutable UnallocatedMap<TaskID, MessageID> task_message_id_;
    OTZMQListenCallback account_subscriber_callback_;
    OTZMQSubscribeSocket account_subscriber_;
    OTZMQListenCallback notification_listener_callback_;
    OTZMQPullSocket notification_listener_;
    OTZMQListenCallback find_nym_callback_;
    OTZMQPullSocket find_nym_listener_;
    OTZMQListenCallback find_server_callback_;
    OTZMQPullSocket find_server_listener_;
    OTZMQListenCallback find_unit_callback_;
    OTZMQPullSocket find_unit_listener_;
    OTZMQPublishSocket task_finished_;
    OTZMQPublishSocket messagability_;
    mutable OTFlag auto_process_inbox_;
    mutable std::atomic<TaskID> next_task_id_;
    mutable std::atomic<bool> shutdown_;
    mutable std::mutex shutdown_lock_;
    mutable PasswordPrompt reason_;

    static auto error_task() -> BackgroundTask;
    static auto error_result() -> Result
    {
        return Result{otx::LastReplyStatus::NotSent, nullptr};
    }

    auto add_task(const TaskID taskID, const otx::client::ThreadStatus status)
        const -> BackgroundTask;
    auto associate_message_id(
        const identifier::Generic& messageID,
        const TaskID taskID) const -> void final;
    auto can_deposit(
        const OTPayment& payment,
        const identifier::Nym& recipient,
        const identifier::Account& accountIDHint,
        identifier::Notary& depositServer,
        identifier::UnitDefinition& unitID,
        identifier::Account& depositAccount) const
        -> otx::client::Depositability final;
    auto can_message(
        const identifier::Nym& senderNymID,
        const identifier::Generic& recipientContactID,
        identifier::Nym& recipientNymID,
        identifier::Notary& serverID) const -> otx::client::Messagability;
    auto extract_payment_data(
        const OTPayment& payment,
        identifier::Nym& nymID,
        identifier::Notary& serverID,
        identifier::UnitDefinition& unitID) const -> bool;
    auto find_nym(const opentxs::network::zeromq::Message& message) const
        -> void;
    auto find_server(const opentxs::network::zeromq::Message& message) const
        -> void;
    auto find_unit(const opentxs::network::zeromq::Message& message) const
        -> void;
    auto finish_task(const TaskID taskID, const bool success, Result&& result)
        const -> bool final;
    auto get_introduction_server(const Lock& lock) const -> identifier::Notary;
    auto get_nym_fetch(const identifier::Notary& serverID) const
        -> UniqueQueue<identifier::Nym>& final;
    auto get_operations(const ContextID& id) const noexcept(false)
        -> otx::client::implementation::StateMachine&;
    auto get_task(const ContextID& id) const
        -> otx::client::implementation::StateMachine&;
    auto load_introduction_server(const Lock& lock) const -> void;
    auto next_task_id() const -> TaskID { return ++next_task_id_; }
    auto process_account(const opentxs::network::zeromq::Message& message) const
        -> void;
    auto process_notification(
        const opentxs::network::zeromq::Message& message) const -> void;
    auto publish_messagability(
        const identifier::Nym& sender,
        const identifier::Generic& contact,
        otx::client::Messagability value) const noexcept
        -> otx::client::Messagability;
    auto publish_server_registration(
        const identifier::Nym& nymID,
        const identifier::Notary& serverID,
        const bool forcePrimary) const -> bool;
    auto queue_cheque_deposit(
        const identifier::Nym& nymID,
        const Cheque& cheque) const -> bool;
    auto refresh_accounts() const -> bool;
    auto refresh_contacts() const -> bool;
    auto schedule_download_nymbox(
        const identifier::Nym& localNymID,
        const identifier::Notary& serverID) const -> BackgroundTask;
    auto schedule_register_account(
        const identifier::Nym& localNymID,
        const identifier::Notary& serverID,
        const identifier::UnitDefinition& unitID,
        const UnallocatedCString& label) const -> BackgroundTask;
    auto set_contact(
        const identifier::Nym& nymID,
        const identifier::Notary& serverID) const -> void;
    auto set_introduction_server(
        const Lock& lock,
        const contract::Server& contract) const noexcept -> identifier::Notary;
    auto set_introduction_server(
        const Lock& lock,
        const protobuf::ServerContract& contract) const noexcept
        -> identifier::Notary;
    auto start_task(const TaskID taskID, bool success) const
        -> BackgroundTask final;
    auto status(const Lock& lock, const TaskID taskID) const
        -> otx::client::ThreadStatus;
    auto update_task(
        const TaskID taskID,
        const otx::client::ThreadStatus status,
        Result&& result) const noexcept -> void;
    auto start_introduction_server(const identifier::Nym& nymID) const -> void;
    auto trigger_all() const -> void;
    auto valid_account(
        const OTPayment& payment,
        const identifier::Nym& recipient,
        const identifier::Notary& serverID,
        const identifier::UnitDefinition& unitID,
        const identifier::Account& accountIDHint,
        identifier::Account& depositAccount) const
        -> otx::client::Depositability;
    auto valid_context(
        const identifier::Nym& nymID,
        const identifier::Notary& serverID) const -> bool;
    auto valid_recipient(
        const OTPayment& payment,
        const identifier::Nym& specifiedNymID,
        const identifier::Nym& recipient) const -> otx::client::Depositability;
};
}  // namespace opentxs::api::session::imp
