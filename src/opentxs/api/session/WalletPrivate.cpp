// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include <cxxabi.h>

#include "opentxs/api/session/WalletPrivate.hpp"  // IWYU pragma: associated

#include <opentxs/protobuf/Context.pb.h>
#include <opentxs/protobuf/Credential.pb.h>
#include <opentxs/protobuf/Issuer.pb.h>  // IWYU pragma: keep
#include <opentxs/protobuf/Nym.pb.h>
#include <opentxs/protobuf/PeerReply.pb.h>
#include <opentxs/protobuf/PeerRequest.pb.h>
#include <opentxs/protobuf/Purse.pb.h>
#include <opentxs/protobuf/ServerContract.pb.h>
#include <opentxs/protobuf/UnitDefinition.pb.h>
#include <algorithm>
#include <compare>
#include <functional>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

#include "internal/api/session/Endpoints.hpp"
#include "internal/api/session/Storage.hpp"
#include "internal/core/Core.hpp"
#include "internal/core/String.hpp"
#include "internal/core/contract/BasketContract.hpp"
#include "internal/core/contract/CurrencyContract.hpp"  // IWYU pragma: keep
#include "internal/core/contract/SecurityContract.hpp"  // IWYU pragma: keep
#include "internal/core/contract/Unit.hpp"
#include "internal/core/contract/peer/Object.hpp"
#include "internal/core/contract/peer/Reply.hpp"
#include "internal/core/contract/peer/Request.hpp"
#include "internal/identity/Nym.hpp"
#include "internal/network/otdht/Factory.hpp"
#include "internal/network/zeromq/Batch.hpp"
#include "internal/network/zeromq/Context.hpp"
#include "internal/network/zeromq/ListenCallback.hpp"
#include "internal/network/zeromq/message/Message.hpp"
#include "internal/network/zeromq/socket/Factory.hpp"
#include "internal/network/zeromq/socket/Push.hpp"
#include "internal/otx/blind/Factory.hpp"
#include "internal/otx/blind/Purse.hpp"
#include "internal/otx/client/Factory.hpp"
#include "internal/otx/client/Issuer.hpp"
#include "internal/otx/common/Account.hpp"
#include "internal/otx/common/NymFile.hpp"
#include "internal/otx/common/XML.hpp"
#include "internal/otx/consensus/Consensus.hpp"
#include "internal/otx/consensus/Server.hpp"
#include "internal/util/Exclusive.hpp"
#include "internal/util/Pimpl.hpp"
#include "internal/util/Shared.hpp"
#include "internal/util/SharedPimpl.hpp"
#include "opentxs/Time.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/UnitType.hpp"  // IWYU pragma: keep
#include "opentxs/WorkType.internal.hpp"
#include "opentxs/api/Factory.internal.hpp"
#include "opentxs/api/Network.hpp"
#include "opentxs/api/Session.hpp"
#include "opentxs/api/Session.internal.hpp"
#include "opentxs/api/network/ZeroMQ.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Factory.internal.hpp"
#include "opentxs/api/session/Storage.hpp"
#include "opentxs/blockchain/Type.hpp"  // IWYU pragma: keep
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/block/Hash.hpp"  // IWYU pragma: keep
#include "opentxs/contract/ContractType.hpp"
#include "opentxs/contract/Types.hpp"
#include "opentxs/contract/Types.internal.hpp"
#include "opentxs/core/Amount.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/contract/peer/ObjectType.hpp"  // IWYU pragma: keep
#include "opentxs/core/contract/peer/Reply.hpp"
#include "opentxs/core/contract/peer/Request.hpp"
#include "opentxs/core/contract/peer/Types.hpp"
#include "opentxs/crypto/Parameters.hpp"  // IWYU pragma: keep
#include "opentxs/display/Definition.hpp"
#include "opentxs/identifier/Notary.hpp"
#include "opentxs/identifier/Nym.hpp"
#include "opentxs/identifier/Types.hpp"
#include "opentxs/identifier/UnitDefinition.hpp"
#include "opentxs/identity/IdentityType.hpp"  // IWYU pragma: keep
#include "opentxs/identity/Nym.hpp"
#include "opentxs/internal.factory.hpp"
#include "opentxs/network/otdht/Base.hpp"
#include "opentxs/network/otdht/MessageType.hpp"  // IWYU pragma: keep
#include "opentxs/network/otdht/PublishContract.hpp"
#include "opentxs/network/otdht/PublishContractReply.hpp"
#include "opentxs/network/otdht/QueryContract.hpp"
#include "opentxs/network/otdht/QueryContractReply.hpp"
#include "opentxs/network/otdht/Types.hpp"
#include "opentxs/network/otdht/Types.internal.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/Types.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/message/Message.tpp"
#include "opentxs/network/zeromq/socket/Direction.hpp"   // IWYU pragma: keep
#include "opentxs/network/zeromq/socket/SocketType.hpp"  // IWYU pragma: keep
#include "opentxs/network/zeromq/socket/Types.hpp"
#include "opentxs/otx/ConsensusType.hpp"  // IWYU pragma: keep
#include "opentxs/otx/Types.hpp"
#include "opentxs/otx/Types.internal.hpp"
#include "opentxs/otx/blind/Purse.hpp"
#include "opentxs/otx/client/StorageBox.hpp"  // IWYU pragma: keep
#include "opentxs/otx/client/Types.hpp"
#include "opentxs/protobuf/Types.internal.tpp"
#include "opentxs/protobuf/syntax/Nym.hpp"
#include "opentxs/protobuf/syntax/Purse.hpp"
#include "opentxs/protobuf/syntax/ServerContract.hpp"
#include "opentxs/protobuf/syntax/Types.internal.tpp"
#include "opentxs/protobuf/syntax/UnitDefinition.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/NymEditor.hpp"
#include "opentxs/util/Writer.hpp"
#include "util/Exclusive.tpp"

template class opentxs::Exclusive<opentxs::Account>;
template class opentxs::Shared<opentxs::Account>;

namespace opentxs::api::session
{
WalletPrivate::WalletPrivate(const api::Session& api)
    : self_(this)
    , api_(api)
    , context_map_()
    , account_map_()
    , nym_map_()
    , server_map_()
    , unit_map_()
    , issuer_map_()
    , create_nym_lock_()
    , account_map_lock_()
    , nym_map_lock_()
    , server_map_lock_()
    , unit_map_lock_()
    , issuer_map_lock_()
    , peer_map_lock_()
    , peer_lock_()
    , nymfile_map_lock_()
    , nymfile_lock_()
    , purse_lock_()
    , purse_map_()
    , account_publisher_(
          api_.Network().ZeroMQ().Context().Internal().PublishSocket())
    , issuer_publisher_(
          api_.Network().ZeroMQ().Context().Internal().PublishSocket())
    , nym_publisher_(
          api_.Network().ZeroMQ().Context().Internal().PublishSocket())
    , nym_created_publisher_(
          api_.Network().ZeroMQ().Context().Internal().PublishSocket())
    , server_publisher_(
          api_.Network().ZeroMQ().Context().Internal().PublishSocket())
    , unit_publisher_(
          api_.Network().ZeroMQ().Context().Internal().PublishSocket())
    , peer_reply_publisher_(
          api_.Network().ZeroMQ().Context().Internal().PublishSocket())
    , peer_reply_new_publisher_(
          api_.Network().ZeroMQ().Context().Internal().PublishSocket())
    , peer_request_publisher_(
          api_.Network().ZeroMQ().Context().Internal().PublishSocket())
    , peer_request_new_publisher_(
          api_.Network().ZeroMQ().Context().Internal().PublishSocket())
    , find_nym_(api_.Network().ZeroMQ().Context().Internal().PushSocket(
          opentxs::network::zeromq::socket::Direction::Connect))
    , handle_([&] {
        using Type = opentxs::network::zeromq::socket::Type;

        return api_.Network().ZeroMQ().Context().Internal().MakeBatch(
            {
                Type::Router,  // NOTE p2p_socket_
                Type::Pull,    // NOTE loopback_
            },
            "api::session::Wallet");
    }())
    , batch_([&]() -> auto& {
        using Callback = opentxs::network::zeromq::ListenCallback;
        auto& out = handle_.batch_;
        out.listen_callbacks_.emplace_back(Callback::Factory(
            [this](auto&& in) { process_p2p(std::move(in)); }));

        return out;
    }())
    , p2p_callback_(batch_.listen_callbacks_.at(0))
    , p2p_socket_([&]() -> auto& {
        auto& out = batch_.sockets_.at(0);
        const auto endpoint =
            CString{api_.Endpoints().Internal().OTDHTWallet()};
        const auto rc = out.Bind(endpoint.c_str());

        assert_true(rc);

        LogTrace()()("wallet socket bound to ")(endpoint).Flush();

        return out;
    }())
    , loopback_(batch_.sockets_.at(1))
    , to_loopback_([&] {
        using Type = opentxs::network::zeromq::socket::Type;
        const auto endpoint = opentxs::network::zeromq::MakeArbitraryInproc();
        const auto& context = api_.Network().ZeroMQ().Context();
        auto socket = factory::ZMQSocket(context, Type::Push);
        auto rc = loopback_.Bind(endpoint.c_str());

        assert_true(rc);

        rc = socket.Connect(endpoint.c_str());

        assert_true(rc);

        return socket;
    }())
    , thread_(api_.Network().ZeroMQ().Context().Internal().Start(
          batch_.id_,
          {
              {p2p_socket_.ID(),
               &p2p_socket_,
               [id = p2p_socket_.ID(), &cb = p2p_callback_](auto&& m) {
                   cb.Process(std::move(m));
               }},
              {loopback_.ID(),
               &loopback_,
               [id = loopback_.ID(), &socket = p2p_socket_, &batch = batch_](
                   auto&& m) {
                   if (batch.toggle_) { socket.Send(std::move(m)); }
               }},
          }))
{
    LogTrace()()("using ZMQ batch ")(batch_.id_).Flush();
    account_publisher_->Start(api_.Endpoints().AccountUpdate().data());
    issuer_publisher_->Start(api_.Endpoints().IssuerUpdate().data());
    nym_publisher_->Start(api_.Endpoints().NymDownload().data());
    nym_created_publisher_->Start(api_.Endpoints().NymCreated().data());
    server_publisher_->Start(api_.Endpoints().ServerUpdate().data());
    unit_publisher_->Start(api_.Endpoints().UnitUpdate().data());
    peer_reply_publisher_->Start(
        api_.Endpoints().Internal().PeerReplyUpdate().data());
    peer_reply_new_publisher_->Start(api_.Endpoints().PeerReply().data());
    peer_request_publisher_->Start(
        api_.Endpoints().Internal().PeerRequestUpdate().data());
    peer_request_new_publisher_->Start(api_.Endpoints().PeerRequest().data());
    find_nym_->Start(api_.Endpoints().FindNym().data());

    assert_false(nullptr == thread_);
}

auto WalletPrivate::account(
    const Lock& lock,
    const identifier::Account& account,
    const bool create) const -> WalletPrivate::AccountLock&
{
    assert_true(CheckLock(lock, account_map_lock_));

    auto& row = account_map_[account];
    auto& [rowMutex, pAccount] = row;

    if (pAccount) {
        LogVerbose()()("Account ")(account, api_.Crypto())(
            " already exists in map.")
            .Flush();

        return row;
    }

    const auto rowLock = eLock{rowMutex};
    // What if more than one thread tries to create the same row at the same
    // time? One thread will construct the Account object and the other(s) will
    // block until the lock is obtained. Therefore this check is necessary to
    // avoid creating the same account twice.
    if (pAccount) { return row; }

    UnallocatedCString serialized{""};
    UnallocatedCString alias{""};
    using enum opentxs::storage::ErrorReporting;
    const auto loaded =
        api_.Storage().Internal().Load(account, serialized, alias, silent);

    if (loaded) {
        LogVerbose()()("Account ")(account, api_.Crypto())(
            " loaded from storage.")
            .Flush();
        pAccount.reset(account_factory(account, alias, serialized));

        assert_false(nullptr == pAccount);
    } else {
        if (false == create) {
            LogDetail()()("Trying to load account ")(account, api_.Crypto())(
                " via legacy method.")
                .Flush();
            const auto legacy = load_legacy_account(account, rowLock, row);

            if (legacy) { return row; }

            throw std::out_of_range("Unable to load account from storage");
        }
    }

    return row;
}

auto WalletPrivate::Account(const identifier::Account& accountID) const
    -> SharedAccount
{
    const auto mapLock = Lock{account_map_lock_};

    try {
        auto& [rowMutex, pAccount] = account(mapLock, accountID, false);

        if (pAccount) { return {pAccount.get(), rowMutex}; }
    } catch (...) {

        return {};
    }

    return {};
}

auto WalletPrivate::account_alias(
    const UnallocatedCString& accountID,
    const UnallocatedCString& hint) const -> UnallocatedCString
{
    if (false == hint.empty()) { return hint; }

    return api_.Storage().Internal().AccountAlias(
        api_.Factory().AccountIDFromBase58(accountID));
}

auto WalletPrivate::account_factory(
    const identifier::Account& accountID,
    std::string_view alias,
    const UnallocatedCString& serialized) const -> opentxs::Account*
{
    auto strContract = String::Factory(), strFirstLine = String::Factory();
    const bool bProcessed = DearmorAndTrim(
        api_.Crypto(),
        String::Factory(serialized.c_str()),
        strContract,
        strFirstLine);

    if (false == bProcessed) {
        LogError()()("Failed to dearmor serialized account.").Flush();

        return nullptr;
    }

    const auto owner = api_.Storage().Internal().AccountOwner(accountID);
    const auto notary = api_.Storage().Internal().AccountServer(accountID);

    std::unique_ptr<opentxs::Account> pAccount{
        new opentxs::Account{api_, owner, accountID, notary}};

    if (false == bool(pAccount)) {
        LogError()()("Failed to create account.").Flush();

        return nullptr;
    }

    auto& account = *pAccount;

    if (account.GetNymID() != owner) {
        LogError()()("Nym id (")(account.GetNymID(), api_.Crypto())(
            ") does not match expect value (")(owner, api_.Crypto())(")")
            .Flush();
        account.SetNymID(owner);
    }

    if (account.GetRealAccountID() != accountID) {
        LogError()()("Account id (")(account.GetRealAccountID(), api_.Crypto())(
            ") does not match expect value (")(accountID, api_.Crypto())(")")
            .Flush();
        account.SetRealAccountID(accountID);
    }

    if (account.GetPurportedAccountID() != accountID) {
        LogError()()("Purported account id (")(
            account.GetPurportedAccountID(), api_.Crypto())(
            ") does not match expect value (")(accountID, api_.Crypto())(")")
            .Flush();
        account.SetPurportedAccountID(accountID);
    }

    if (account.GetRealNotaryID() != notary) {
        LogError()()("Notary id (")(account.GetRealNotaryID(), api_.Crypto())(
            ") does not match expect value (")(notary, api_.Crypto())(")")
            .Flush();
        account.SetRealNotaryID(notary);
    }

    if (account.GetPurportedNotaryID() != notary) {
        LogError()()("Purported notary id (")(
            account.GetPurportedNotaryID(), api_.Crypto())(
            ") does not match expect value (")(notary, api_.Crypto())(")")
            .Flush();
        account.SetPurportedNotaryID(notary);
    }

    account.SetLoadInsecure();
    auto deserialized = account.LoadContractFromString(strContract);

    if (false == deserialized) {
        LogError()()("Failed to deserialize account.").Flush();

        return nullptr;
    }

    const auto signerID = api_.Storage().Internal().AccountSigner(accountID);

    if (signerID.empty()) {
        LogError()()("Unknown signer nym.").Flush();

        return nullptr;
    }

    const auto signerNym = Nym(signerID);

    if (false == bool(signerNym)) {
        LogError()()("Unable to load signer nym.").Flush();

        return nullptr;
    }

    if (false == account.VerifySignature(*signerNym)) {
        LogError()()("Invalid signature.").Flush();

        return nullptr;
    }

    account.SetAlias(alias);

    return pAccount.release();
}

auto WalletPrivate::AccountPartialMatch(const UnallocatedCString& hint) const
    -> identifier::Generic
{
    const auto list = api_.Storage().Internal().AccountList();

    for (const auto& [id, alias] : list) {
        if (0 == id.compare(0, hint.size(), hint)) {

            return api_.Factory().IdentifierFromBase58(id);
        }

        if (0 == alias.compare(0, hint.size(), hint)) {

            return api_.Factory().IdentifierFromBase58(alias);
        }
    }

    return {};
}

auto WalletPrivate::BasketContract(
    const identifier::UnitDefinition& id,
    const std::chrono::milliseconds& timeout) const noexcept(false)
    -> OTBasketContract
{
    UnitDefinition(id, timeout);

    const auto mapLock = Lock{unit_map_lock_};
    auto it = unit_map_.find(id);

    if (unit_map_.end() == it) {
        throw std::runtime_error("Basket contract ID not found");
    }

    auto output = std::dynamic_pointer_cast<contract::unit::Basket>(it->second);

    if (output) {

        return OTBasketContract{std::move(output)};
    } else {
        throw std::runtime_error("Unit definition is not a basket contract");
    }
}

auto WalletPrivate::CreateAccount(
    const identifier::Nym& ownerNymID,
    const identifier::Notary& notaryID,
    const identifier::UnitDefinition& instrumentDefinitionID,
    const identity::Nym& signer,
    Account::AccountType type,
    TransactionNumber stash,
    const PasswordPrompt& reason) const -> ExclusiveAccount
{
    const auto mapLock = Lock{account_map_lock_};

    try {
        const auto contract = UnitDefinition(instrumentDefinitionID);
        std::unique_ptr<opentxs::Account> newAccount(
            opentxs::Account::GenerateNewAccount(
                api_,
                signer.ID(),
                notaryID,
                signer,
                ownerNymID,
                instrumentDefinitionID,
                reason,
                type,
                stash));

        assert_false(nullptr == newAccount);

        const auto& accountID = newAccount->GetRealAccountID();
        auto& [rowMutex, pAccount] = account(mapLock, accountID, true);

        if (pAccount) {
            LogError()()("Account already exists.").Flush();

            return {};
        } else {
            pAccount = std::move(newAccount);

            assert_false(nullptr == pAccount);

            pAccount->SetNymID(ownerNymID);
            pAccount->SetPurportedAccountID(accountID);
            pAccount->SetRealNotaryID(notaryID);
            pAccount->SetPurportedNotaryID(notaryID);
            auto serialized = String::Factory();
            pAccount->SaveContractRaw(serialized);
            const auto saved = api_.Storage().Internal().Store(
                accountID,
                serialized->Get(),
                "",
                ownerNymID,
                signer.ID(),
                contract->Signer()->ID(),
                notaryID,
                instrumentDefinitionID,
                extract_unit(instrumentDefinitionID));

            assert_true(saved);

            const std::function<void(
                std::unique_ptr<opentxs::Account>&, eLock&, bool)>
                callback = [this, accountID, &reason](
                               std::unique_ptr<opentxs::Account>& in,
                               eLock& lock,
                               bool success) -> void {
                this->save(reason, accountID, in, lock, success);
            };

            return {&pAccount, rowMutex, callback};
        }
    } catch (...) {

        return {};
    }
}

auto WalletPrivate::DefaultNym() const noexcept
    -> std::pair<identifier::Nym, std::size_t>
{
    auto lock = Lock{create_nym_lock_};

    return std::make_pair(api_.Storage().DefaultNym(), LocalNymCount());
}

auto WalletPrivate::DeleteAccount(const identifier::Account& accountID) const
    -> bool
{
    const auto mapLock = Lock{account_map_lock_};

    try {
        auto& [rowMutex, pAccount] = account(mapLock, accountID, false);
        const auto lock = eLock{rowMutex};

        if (pAccount) {
            const auto deleted =
                api_.Storage().Internal().DeleteAccount(accountID);

            if (deleted) {
                pAccount.reset();

                return true;
            }
        }
    } catch (...) {

        return false;
    }

    return false;
}

auto WalletPrivate::IssuerAccount(
    const identifier::UnitDefinition& unitID) const -> SharedAccount
{
    const auto accounts = api_.Storage().Internal().AccountsByContract(unitID);
    const auto mapLock = Lock{account_map_lock_};

    try {
        for (const auto& accountID : accounts) {
            auto& [rowMutex, pAccount] = account(mapLock, accountID, false);

            if (pAccount) {
                if (pAccount->IsIssuer()) { return {pAccount.get(), rowMutex}; }
            }
        }
    } catch (...) {

        return {};
    }

    return {};
}

auto WalletPrivate::mutable_Account(
    const identifier::Account& accountID,
    const PasswordPrompt& reason,
    const AccountCallback callback) const -> ExclusiveAccount
{
    const auto mapLock = Lock{account_map_lock_};

    try {
        auto& [rowMutex, pAccount] = account(mapLock, accountID, false);

        if (pAccount) {
            const std::function<void(
                std::unique_ptr<opentxs::Account>&, eLock&, bool)>
                save = [this, accountID, &reason](
                           std::unique_ptr<opentxs::Account>& in,
                           eLock& lock,
                           bool success) -> void {
                this->save(reason, accountID, in, lock, success);
            };

            return {&pAccount, rowMutex, save, callback};
        }
    } catch (...) {

        return {};
    }

    return {};
}

auto WalletPrivate::UpdateAccount(
    const identifier::Account& accountID,
    const otx::context::Server& context,
    const String& serialized,
    const PasswordPrompt& reason) const -> bool
{
    return UpdateAccount(accountID, context, serialized, "", reason);
}

auto WalletPrivate::UpdateAccount(
    const identifier::Account& accountID,
    const otx::context::Server& context,
    const String& serialized,
    const UnallocatedCString& label,
    const PasswordPrompt& reason) const -> bool
{
    auto mapLock = Lock{account_map_lock_};
    auto& [rowMutex, pAccount] = account(mapLock, accountID, true);
    const auto rowLock = eLock{rowMutex};
    mapLock.unlock();
    const auto& localNym = *context.Signer();
    std::unique_ptr<opentxs::Account> newAccount{nullptr};
    newAccount.reset(
        new opentxs::Account(api_, localNym.ID(), accountID, context.Notary()));

    if (false == bool(newAccount)) {
        LogError()()("Unable to construct account.").Flush();

        return false;
    }

    if (false == newAccount->LoadContractFromString(serialized)) {
        LogError()()("Unable to deserialize account.").Flush();

        return false;
    }

    if (false == newAccount->VerifyAccount(context.RemoteNym())) {
        LogError()()("Unable to verify account.").Flush();

        return false;
    }

    if (localNym.ID() != newAccount->GetNymID()) {
        LogError()()("Wrong nym on account.").Flush();

        return false;
    }

    if (context.Notary() != newAccount->GetRealNotaryID()) {
        LogError()()("Wrong server on account.").Flush();

        return false;
    }

    newAccount->ReleaseSignatures();

    if (false == newAccount->SignContract(localNym, reason)) {
        LogError()()("Unable to sign account.").Flush();

        return false;
    }

    if (false == newAccount->SaveContract()) {
        LogError()()("Unable to serialize account.").Flush();

        return false;
    }

    pAccount = std::move(newAccount);

    assert_false(nullptr == pAccount);

    const auto& unitID = pAccount->GetInstrumentDefinitionID();

    try {
        const auto contract = UnitDefinition(unitID);
        auto raw = String::Factory();
        auto saved = pAccount->SaveContractRaw(raw);

        if (false == saved) {
            LogError()()("Unable to serialize account.").Flush();

            return false;
        }

        const auto alias =
            account_alias(accountID.asBase58(api_.Crypto()), label);
        saved = api_.Storage().Internal().Store(
            accountID,
            raw->Get(),
            alias,
            localNym.ID(),
            localNym.ID(),
            contract->Signer()->ID(),
            context.Notary(),
            unitID,
            extract_unit(contract));

        if (false == saved) {
            LogError()()("Unable to save account.").Flush();

            return false;
        }

        pAccount->SetAlias(alias);
        const auto balance = pAccount->GetBalance();
        account_publisher_->Send([&] {
            auto work = opentxs::network::zeromq::tagged_message(
                WorkType::AccountUpdated, true);
            accountID.Serialize(work);
            balance.Serialize(work.AppendBytes());

            return work;
        }());

        return true;
    } catch (...) {
        LogError()()("Unable to load unit definition contract ")(
            unitID, api_.Crypto())
            .Flush();

        return false;
    }
}

auto WalletPrivate::CurrencyTypeBasedOnUnitType(
    const identifier::UnitDefinition& contractID) const -> UnitType
{
    return extract_unit(contractID);
}

auto WalletPrivate::extract_unit(
    const identifier::UnitDefinition& contractID) const -> UnitType
{
    try {
        const auto contract = UnitDefinition(contractID);

        return extract_unit(contract);
    } catch (...) {
        LogError()()(" Unable to load unit definition contract ")(
            contractID, api_.Crypto())(".")
            .Flush();

        return UnitType::Unknown;
    }
}

auto WalletPrivate::extract_unit(const contract::Unit& contract) const
    -> UnitType
{
    try {
        return contract.UnitOfAccount();
    } catch (...) {

        return UnitType::Unknown;
    }
}

auto WalletPrivate::context(
    const identifier::Nym& localNymID,
    const identifier::Nym& remoteNymID,
    ContextMap& map) const -> std::shared_ptr<otx::context::Base>
{
    const auto local = localNymID.asBase58(api_.Crypto());
    const auto remote = remoteNymID.asBase58(api_.Crypto());
    const ContextID context = {local, remote};
    auto it = map.find(context);
    const bool inMap = (it != map.end());

    if (inMap) { return it->second; }

    // Load from storage, if it exists.
    auto serialized = protobuf::Context{};
    using enum opentxs::storage::ErrorReporting;
    const bool loaded = api_.Storage().Internal().Load(
        localNymID, remoteNymID, serialized, silent);

    if (!loaded) { return nullptr; }

    auto expected = api_.Factory().Internal().NymID(serialized.localnym());

    if (localNymID != expected) {
        LogError()()("Incorrect localnym in protobuf.").Flush();

        return nullptr;
    }

    expected = api_.Factory().Internal().NymID(serialized.remotenym());

    if (remoteNymID != expected) {
        LogError()()("Incorrect localnym in protobuf.").Flush();

        return nullptr;
    }

    auto& entry = map[context];

    // Obtain nyms.
    const auto localNym = Nym(localNymID);
    const auto remoteNym = Nym(remoteNymID);

    if (!localNym) {
        LogError()()("Unable to load local nym.").Flush();

        return nullptr;
    }

    if (!remoteNym) {
        LogError()()("Unable to load remote nym.").Flush();

        return nullptr;
    }

    switch (otx::translate(serialized.type())) {
        case otx::ConsensusType::Server: {
            instantiate_server_context(serialized, localNym, remoteNym, entry);
        } break;
        case otx::ConsensusType::Client: {
            instantiate_client_context(serialized, localNym, remoteNym, entry);
        } break;
        case otx::ConsensusType::Error:
        case otx::ConsensusType::Peer:
        default: {
            return nullptr;
        }
    }

    assert_false(nullptr == entry);

    if (false == entry->Validate()) {
        map.erase(context);
        LogAbort()()("Invalid signature on context.").Abort();
    }

    return entry;
}

auto WalletPrivate::ClientContext(const identifier::Nym& remoteNymID) const
    -> std::shared_ptr<const otx::context::Client>
{
    // Overridden in appropriate child class.
    LogAbort()().Abort();
}

auto WalletPrivate::ServerContext(
    const identifier::Nym& localNymID,
    const identifier::Generic& remoteID) const
    -> std::shared_ptr<const otx::context::Server>
{
    // Overridden in appropriate child class.
    LogAbort()().Abort();
}

auto WalletPrivate::mutable_ClientContext(
    const identifier::Nym& remoteNymID,
    const PasswordPrompt& reason) const -> Editor<otx::context::Client>
{
    // Overridden in appropriate child class.
    LogAbort()().Abort();
}

auto WalletPrivate::mutable_ServerContext(
    const identifier::Nym& localNymID,
    const identifier::Generic& remoteID,
    const PasswordPrompt& reason) const -> Editor<otx::context::Server>
{
    // Overridden in appropriate child class.
    LogAbort()().Abort();
}

auto WalletPrivate::ImportAccount(
    std::unique_ptr<opentxs::Account>& imported) const -> bool
{
    if (false == bool(imported)) {
        LogError()()("Invalid account.").Flush();

        return false;
    }

    const auto& accountID = imported->GetRealAccountID();
    auto mapLock = Lock{account_map_lock_};

    try {
        auto& [rowMutex, pAccount] = account(mapLock, accountID, true);
        const auto rowLock = eLock{rowMutex};
        mapLock.unlock();

        if (pAccount) {
            LogError()()("Account already exists.").Flush();

            return false;
        }

        pAccount = std::move(imported);

        assert_false(nullptr == pAccount);

        const auto& contractID = pAccount->GetInstrumentDefinitionID();

        try {
            const auto contract = UnitDefinition(contractID);
            auto serialized = String::Factory();
            auto alias = String::Factory();
            pAccount->SaveContractRaw(serialized);
            pAccount->GetName(alias);
            const auto saved = api_.Storage().Internal().Store(
                accountID,
                serialized->Get(),
                alias->Get(),
                pAccount->GetNymID(),
                pAccount->GetNymID(),
                contract->Signer()->ID(),
                pAccount->GetRealNotaryID(),
                contractID,
                extract_unit(contract));

            if (false == saved) {
                LogError()()("Failed to save account.").Flush();
                imported = std::move(pAccount);

                return false;
            }

            return true;
        } catch (...) {
            LogError()()("Unable to load unit definition.").Flush();
            imported = std::move(pAccount);

            return false;
        }
    } catch (...) {
    }

    LogError()()("Unable to import account.").Flush();

    return false;
}

auto WalletPrivate::IssuerList(const identifier::Nym& nymID) const
    -> UnallocatedSet<identifier::Nym>
{
    UnallocatedSet<identifier::Nym> output{};
    auto list = api_.Storage().Internal().IssuerList(nymID);

    for (const auto& it : list) {
        output.emplace(api_.Factory().NymIDFromBase58(it.first));
    }

    return output;
}

auto WalletPrivate::Issuer(
    const identifier::Nym& nymID,
    const identifier::Nym& issuerID) const
    -> std::shared_ptr<const otx::client::Issuer>
{
    auto& [lock, pIssuer] = issuer(nymID, issuerID, false);

    return pIssuer;
}

auto WalletPrivate::mutable_Issuer(
    const identifier::Nym& nymID,
    const identifier::Nym& issuerID) const -> Editor<otx::client::Issuer>
{
    auto& [mutex, pIssuer] = issuer(nymID, issuerID, true);

    assert_false(nullptr == pIssuer);

    const std::function<void(otx::client::Issuer*, const Lock&)> callback =
        [=, this](otx::client::Issuer* in, const Lock& lock) -> void {
        save(lock, in);
    };

    return {mutex, pIssuer.get(), callback};
}

auto WalletPrivate::issuer(
    const identifier::Nym& nymID,
    const identifier::Nym& issuerID,
    const bool create) const -> WalletPrivate::IssuerLock&
{
    const auto lock = Lock{issuer_map_lock_};
    const auto key = IssuerID{nymID, issuerID};
    auto& output = issuer_map_[key];
    auto& [issuerMutex, pIssuer] = output;

    if (pIssuer) { return output; }

    const auto isBlockchain =
        (blockchain::Type::UnknownBlockchain !=
         blockchain::Chain(api_, issuerID));

    if (isBlockchain) {
        LogError()()(
            " erroneously attempting to load a blockchain as an otx issuer")
            .Flush();
    }

    auto serialized = protobuf::Issuer{};
    using enum opentxs::storage::ErrorReporting;
    const bool loaded =
        api_.Storage().Internal().Load(nymID, issuerID, serialized, silent);

    if (loaded) {
        if (isBlockchain) {
            LogError()()("deleting invalid issuer").Flush();
            // TODO
        } else {
            pIssuer.reset(factory::Issuer(
                api_.Crypto(), api_.Factory(), Self(), nymID, serialized));

            assert_false(nullptr == pIssuer);

            return output;
        }
    }

    if (create && (!isBlockchain)) {
        pIssuer.reset(factory::Issuer(
            api_.Crypto(), api_.Factory(), Self(), nymID, issuerID));

        assert_false(nullptr == pIssuer);

        save(lock, pIssuer.get());

        return output;
    }

    issuer_map_.erase(key);
    static auto blank = IssuerLock{};

    return blank;
}

auto WalletPrivate::IsLocalNym(const std::string_view id) const -> bool
{
    return IsLocalNym(api_.Factory().NymIDFromBase58(id));
}

auto WalletPrivate::IsLocalNym(const identifier::Nym& id) const -> bool
{
    return api_.Storage().LocalNyms().contains(id);
}

auto WalletPrivate::LocalNymCount() const -> std::size_t
{
    return api_.Storage().LocalNyms().size();
}

auto WalletPrivate::LocalNyms() const -> Set<identifier::Nym>
{
    return api_.Storage().LocalNyms();
}

auto WalletPrivate::Nym(
    const identifier::Nym& id,
    const std::chrono::milliseconds& timeout) const -> Nym_p
{
    if (blockchain::Type::UnknownBlockchain != blockchain::Chain(api_, id)) {
        LogError()()(" erroneously attempting to load a blockchain as a nym")
            .Flush();

        return nullptr;
    }

    auto mapLock = Lock{nym_map_lock_};
    const bool inMap = (nym_map_.find(id) != nym_map_.end());
    bool valid = false;

    if (!inMap) {
        auto serialized = protobuf::Nym{};
        auto alias = UnallocatedCString{};
        using enum opentxs::storage::ErrorReporting;
        const bool loaded =
            api_.Storage().Internal().Load(id, serialized, alias, silent);

        if (loaded) {
            auto& pNym = nym_map_[id].second;
            pNym.reset(opentxs::Factory::Nym(api_, serialized, alias));

            if (pNym && pNym->CompareID(id)) {
                valid = pNym->VerifyPseudonym();
                pNym->SetAliasStartup(alias);
            } else {
                nym_map_.erase(id);
            }
        } else {
            search_nym(id);

            if (timeout > 0ms) {
                mapLock.unlock();
                auto start = std::chrono::high_resolution_clock::now();
                auto end = start + timeout;
                const auto interval = 100ms;

                while (std::chrono::high_resolution_clock::now() < end) {
                    std::this_thread::sleep_for(interval);
                    mapLock.lock();
                    const bool found = (nym_map_.find(id) != nym_map_.end());
                    mapLock.unlock();

                    if (found) { break; }
                }

                return Nym(id);  // timeout of zero prevents infinite
                                 // recursion
            }
        }
    } else {
        auto& pNym = nym_map_[id].second;
        if (pNym) { valid = pNym->VerifyPseudonym(); }
    }

    if (valid) { return nym_map_[id].second; }

    return nullptr;
}

auto WalletPrivate::Nym(const protobuf::Nym& serialized) const -> Nym_p
{
    const auto nymID = api_.Factory().Internal().NymID(serialized.id());

    if (nymID.empty()) {
        LogError()()("Invalid nym ID.").Flush();

        return {};
    }

    auto existing = Nym(nymID);

    if (existing && (existing->Revision() >= serialized.revision())) {
        LogDetail()()(" Incoming nym is not newer than existing nym.").Flush();

        return existing;
    } else {
        auto pCandidate = std::unique_ptr<identity::internal::Nym>{
            opentxs::Factory::Nym(api_, serialized, "")};

        if (false == bool(pCandidate)) { return {}; }

        auto& candidate = *pCandidate;

        if (false == candidate.CompareID(nymID)) { return existing; }

        if (candidate.VerifyPseudonym()) {
            LogDetail()()("Saving updated nym ")(nymID, api_.Crypto()).Flush();
            candidate.WriteCredentials();
            SaveCredentialIDs(candidate);
            auto mapNym = [&] {
                auto mapLock = Lock{nym_map_lock_};
                auto& out = nym_map_[nymID].second;
                // TODO update existing nym rather than destroying it
                out.reset(pCandidate.release());

                return out;
            }();

            notify_new(nymID);

            return mapNym;
        } else {
            LogError()()("Incoming nym is not valid.").Flush();
        }
    }

    return existing;
}

auto WalletPrivate::Nym(const ReadView& bytes) const -> Nym_p
{
    return Nym(protobuf::Factory<protobuf::Nym>(bytes));
}

auto WalletPrivate::Nym(
    const identity::Type type,
    const PasswordPrompt& reason,
    const UnallocatedCString& name) const -> Nym_p
{
    return Nym({api_.Factory()}, type, reason, name);
}

auto WalletPrivate::Nym(
    const opentxs::crypto::Parameters& parameters,
    const PasswordPrompt& reason,
    const UnallocatedCString& name) const -> Nym_p
{
    return Nym(parameters, identity::Type::individual, reason, name);
}

auto WalletPrivate::Nym(
    const PasswordPrompt& reason,
    const UnallocatedCString& name) const -> Nym_p
{
    return Nym({api_.Factory()}, identity::Type::individual, reason, name);
}

auto WalletPrivate::Nym(
    const opentxs::crypto::Parameters& parameters,
    const identity::Type type,
    const PasswordPrompt& reason,
    const UnallocatedCString& name) const -> Nym_p
{
    auto lock = Lock{create_nym_lock_};
    std::shared_ptr<identity::internal::Nym> pNym(
        opentxs::Factory::Nym(api_, parameters, type, name, reason));

    if (false == bool(pNym)) {
        LogError()()("Failed to create nym").Flush();

        return {};
    }

    const auto first = (0u == LocalNymCount());
    auto& nym = *pNym;
    const auto& id = nym.ID();

    if (nym.VerifyPseudonym()) {
        nym.SetAlias(name);

        {
            auto mapLock = Lock{nym_map_lock_};
            auto it = nym_map_.find(id);

            if (nym_map_.end() != it) { return it->second.second; }
        }

        if (SaveCredentialIDs(nym)) {
            nym_to_contact(nym, name);

            {
                auto nymfile = mutable_nymfile(pNym, pNym, id, reason);
            }

            if (first) {
                LogTrace()()("Marking first created nym as default");
                api_.Storage().Internal().SetDefaultNym(id);
            } else {
                LogTrace()()("Default nym already set").Flush();
            }

            {
                auto mapLock = Lock{nym_map_lock_};
                auto& pMapNym = nym_map_[id].second;
                pMapNym = pNym;
                nym_created_publisher_->Send([&] {
                    auto work = opentxs::network::zeromq::tagged_message(
                        WorkType::NymCreated, true);
                    work.AddFrame(pNym->ID());

                    return work;
                }());
            }

            return pNym;
        } else {
            LogError()()("Failed to save credentials").Flush();

            return {};
        }
    } else {

        return {};
    }
}

auto WalletPrivate::mutable_Nym(
    const identifier::Nym& id,
    const PasswordPrompt& reason) const -> NymData
{
    const UnallocatedCString nym = id.asBase58(api_.Crypto());
    auto exists = Nym(id);

    if (false == bool(exists)) {
        LogError()()("Nym ")(nym)(" not found.").Flush();
    }

    auto mapLock = Lock{nym_map_lock_};
    auto it = nym_map_.find(id);

    if (nym_map_.end() == it) { LogAbort()().Abort(); }

    const std::function<void(NymData*, Lock&)> callback =
        [&](NymData* nymData, Lock& lock) -> void {
        this->save(nymData, lock);
    };

    return {
        api_.Crypto(),
        api_.Factory(),
        it->second.first,
        it->second.second,
        callback};
}

auto WalletPrivate::Nymfile(
    const identifier::Nym& id,
    const PasswordPrompt& reason) const
    -> std::unique_ptr<const opentxs::NymFile>
{
    const auto lock = Lock{nymfile_lock(id)};
    const auto targetNym = Nym(id);
    const auto signerNym = signer_nym(id);

    if (false == bool(targetNym)) { return {}; }
    if (false == bool(signerNym)) { return {}; }

    auto nymfile = std::unique_ptr<opentxs::internal::NymFile>(
        opentxs::Factory::NymFile(api_, targetNym, signerNym));

    assert_false(nullptr == nymfile);

    if (false == nymfile->LoadSignedNymFile(reason)) {
        LogError()()(" Failure calling load_signed_nymfile: ")(
            id, api_.Crypto())(".")
            .Flush();

        return {};
    }

    return nymfile;
}

auto WalletPrivate::mutable_Nymfile(
    const identifier::Nym& id,
    const PasswordPrompt& reason) const -> Editor<opentxs::NymFile>
{
    const auto targetNym = Nym(id);
    const auto signerNym = signer_nym(id);

    return mutable_nymfile(targetNym, signerNym, id, reason);
}

auto WalletPrivate::mutable_nymfile(
    const Nym_p& targetNym,
    const Nym_p& signerNym,
    const identifier::Nym& id,
    const PasswordPrompt& reason) const -> Editor<opentxs::NymFile>
{
    auto nymfile = std::unique_ptr<opentxs::internal::NymFile>(
        opentxs::Factory::NymFile(api_, targetNym, signerNym));

    assert_false(nullptr == nymfile);

    if (false == nymfile->LoadSignedNymFile(reason)) {
        nymfile->SaveSignedNymFile(reason);
    }

    using EditorType = Editor<opentxs::NymFile>;
    const EditorType::LockedSave callback = [&](opentxs::NymFile* in,
                                                Lock& lock) -> void {
        this->save(reason, in, lock);
    };
    const EditorType::OptionalCallback deleter =
        [](const opentxs::NymFile& in) {
            auto* p = &const_cast<opentxs::NymFile&>(in);
            delete p;
        };

    return {nymfile_lock(id), nymfile.release(), callback, deleter};
}

auto WalletPrivate::notify_changed(const identifier::Nym& id) const noexcept
    -> void
{
    nym_publisher_->Send([&] {
        auto work = opentxs::network::zeromq::tagged_message(
            WorkType::NymUpdated, true);
        work.AddFrame(id);

        return work;
    }());
}

auto WalletPrivate::notify_new(const identifier::Nym& id) const noexcept -> void
{
    api_.Internal().NewNym(id);
    notify_changed(id);
}

auto WalletPrivate::nymfile_lock(const identifier::Nym& nymID) const
    -> std::mutex&
{
    auto map_lock = Lock{nymfile_map_lock_};
    auto& output = nymfile_lock_[nymID];
    map_lock.unlock();

    return output;
}

auto WalletPrivate::NymByIDPartialMatch(const UnallocatedCString& hint) const
    -> Nym_p
{
    const auto str =
        api_.Factory().NymIDFromBase58(hint).asBase58(api_.Crypto());

    for (const auto& [id, alias] : api_.Storage().NymList()) {
        const auto match = (id.compare(0, hint.length(), hint) == 0) ||
                           (id.compare(0, str.length(), str) == 0) ||
                           (alias.compare(0, hint.length(), hint) == 0);

        if (match) { return Nym(api_.Factory().NymIDFromBase58(id)); }
    }

    return {};
}

auto WalletPrivate::NymList() const -> ObjectList
{
    return api_.Storage().NymList();
}

auto WalletPrivate::NymNameByIndex(const std::size_t index, String& name) const
    -> bool
{
    const auto nymNames = api_.Storage().LocalNyms();

    if (index < nymNames.size()) {
        std::size_t idx{0};
        for (const auto& nymName : nymNames) {
            if (idx == index) {
                name.Set(String::Factory(nymName, api_.Crypto()));

                return true;
            }

            ++idx;
        }
    }

    return false;
}

auto WalletPrivate::peer_lock(const UnallocatedCString& nymID) const
    -> std::mutex&
{
    auto map_lock = Lock{peer_map_lock_};
    auto& output = peer_lock_[nymID];
    map_lock.unlock();

    return output;
}

auto WalletPrivate::PeerReply(
    const identifier::Nym& id,
    const identifier::Generic& reply,
    otx::client::StorageBox box,
    alloc::Strategy alloc) const noexcept -> contract::peer::Reply
{
    try {
        const auto proto = [&] {
            auto lock = Lock{peer_lock(id.asBase58(api_.Crypto()))};
            auto out = protobuf::PeerReply{};
            using enum opentxs::storage::ErrorReporting;
            const auto loaded =
                api_.Storage().Internal().Load(id, reply, box, out, silent);

            if (false == loaded) {
                throw std::runtime_error{"reply not found"};
            }

            return out;
        }();

        return api_.Factory().Internal().Session().PeerReply(proto, alloc);
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return {alloc.result_};
    }
}

auto WalletPrivate::PeerReplyComplete(
    const identifier::Nym& nym,
    const identifier::Generic& replyID) const -> bool
{
    const auto nymID = nym.asBase58(api_.Crypto());
    auto reply = protobuf::PeerReply{};
    const auto lock = Lock{peer_lock(nymID)};
    using enum opentxs::storage::ErrorReporting;
    const bool haveReply = api_.Storage().Internal().Load(
        nym, replyID, otx::client::StorageBox::SENTPEERREPLY, reply, verbose);

    if (!haveReply) {
        LogError()()("Sent reply not found.").Flush();

        return false;
    }

    // This reply may have been loaded by request id.
    const auto realReplyID = api_.Factory().Internal().Identifier(reply.id());
    const bool savedReply = api_.Storage().Internal().Store(
        reply, nym, otx::client::StorageBox::FINISHEDPEERREPLY);

    if (!savedReply) {
        LogError()()("Failed to save finished reply.").Flush();

        return false;
    }

    const bool removedReply = api_.Storage().Internal().RemoveNymBoxItem(
        nym, otx::client::StorageBox::SENTPEERREPLY, realReplyID);

    if (!removedReply) {
        LogError()()(" Failed to delete finished reply from sent box.").Flush();
    }

    return removedReply;
}

auto WalletPrivate::PeerReplyCreate(
    const identifier::Nym& nym,
    const protobuf::PeerRequest& request,
    const protobuf::PeerReply& reply) const -> bool
{
    const auto nymID = nym.asBase58(api_.Crypto());
    const auto requestID = api_.Factory().Internal().Identifier(request.id());
    const auto cookie = api_.Factory().Internal().Identifier(reply.cookie());
    const auto lock = Lock{peer_lock(nymID)};

    if (cookie != requestID) {
        LogError()()(" Reply cookie does not match request id.").Flush();

        return false;
    }

    if (reply.type() != request.type()) {
        LogError()()(" Reply type does not match request type.").Flush();

        return false;
    }

    const bool createdReply = api_.Storage().Internal().Store(
        reply, nym, otx::client::StorageBox::SENTPEERREPLY);

    if (!createdReply) {
        LogError()()("Failed to save sent reply.").Flush();

        return false;
    }

    const bool processedRequest = api_.Storage().Internal().Store(
        request, nym, otx::client::StorageBox::PROCESSEDPEERREQUEST);

    if (!processedRequest) {
        LogError()()("Failed to save processed request.").Flush();

        return false;
    }

    const bool movedRequest = api_.Storage().Internal().RemoveNymBoxItem(
        nym, otx::client::StorageBox::INCOMINGPEERREQUEST, requestID);

    if (!processedRequest) {
        LogError()()(" Failed to delete processed request from incoming box.")
            .Flush();
    }

    return movedRequest;
}

auto WalletPrivate::PeerReplyCreateRollback(
    const identifier::Nym& nym,
    const identifier::Generic& request,
    const identifier::Generic& reply) const -> bool
{
    const auto nymID = nym.asBase58(api_.Crypto());
    const auto lock = Lock{peer_lock(nymID)};
    const auto replyID = reply.asBase58(api_.Crypto());
    auto requestItem = protobuf::PeerRequest{};
    bool output = true;
    auto notUsed = Time{};
    const bool loadedRequest = api_.Storage().Internal().Load(
        nym,
        request,
        otx::client::StorageBox::PROCESSEDPEERREQUEST,
        requestItem,
        notUsed);

    if (loadedRequest) {
        const bool requestRolledBack = api_.Storage().Internal().Store(
            requestItem, nym, otx::client::StorageBox::INCOMINGPEERREQUEST);

        if (requestRolledBack) {
            const bool purgedRequest =
                api_.Storage().Internal().RemoveNymBoxItem(
                    nym,
                    otx::client::StorageBox::PROCESSEDPEERREQUEST,
                    request);
            if (!purgedRequest) {
                LogError()()(" Failed to delete request from processed box.")
                    .Flush();
                output = false;
            }
        } else {
            LogError()()(" Failed to save request to incoming box.").Flush();
            output = false;
        }
    } else {
        LogError()()(" Did not find the request in the processed box.").Flush();
        output = false;
    }

    const bool removedReply = api_.Storage().Internal().RemoveNymBoxItem(
        nym, otx::client::StorageBox::SENTPEERREPLY, reply);

    if (!removedReply) {
        LogError()()(" Failed to delete reply from sent box.").Flush();
        output = false;
    }

    return output;
}

auto WalletPrivate::PeerReplySent(const identifier::Nym& nym) const
    -> ObjectList
{
    const auto nymID = nym.asBase58(api_.Crypto());
    const auto lock = Lock{peer_lock(nymID)};

    return api_.Storage().Internal().NymBoxList(
        nym, otx::client::StorageBox::SENTPEERREPLY);
}

auto WalletPrivate::PeerReplyIncoming(const identifier::Nym& nym) const
    -> ObjectList
{
    const auto nymID = nym.asBase58(api_.Crypto());
    const auto lock = Lock{peer_lock(nymID)};

    return api_.Storage().Internal().NymBoxList(
        nym, otx::client::StorageBox::INCOMINGPEERREPLY);
}

auto WalletPrivate::PeerReplyFinished(const identifier::Nym& nym) const
    -> ObjectList
{
    const auto nymID = nym.asBase58(api_.Crypto());
    const auto lock = Lock{peer_lock(nymID)};

    return api_.Storage().Internal().NymBoxList(
        nym, otx::client::StorageBox::FINISHEDPEERREPLY);
}

auto WalletPrivate::PeerReplyProcessed(const identifier::Nym& nym) const
    -> ObjectList
{
    const auto nymID = nym.asBase58(api_.Crypto());
    const auto lock = Lock{peer_lock(nymID)};

    return api_.Storage().Internal().NymBoxList(
        nym, otx::client::StorageBox::PROCESSEDPEERREPLY);
}

auto WalletPrivate::PeerReplyReceive(
    const identifier::Nym& nym,
    const PeerObject& object) const -> bool
{
    if (contract::peer::ObjectType::Response != object.Type()) {
        LogError()()("This is not a peer reply.").Flush();

        return false;
    }

    const auto& request = object.Request();
    const auto& reply = object.Reply();

    if (false == request.IsValid()) {
        LogError()()("Invalid request.").Flush();

        return false;
    }

    if (false == reply.IsValid()) {
        LogError()()("Invalid reply.").Flush();

        return false;
    }

    const auto nymID = nym.asBase58(api_.Crypto());
    const auto lock = Lock{peer_lock(nymID)};
    const auto& requestID = request.ID();
    auto serializedRequest = protobuf::PeerRequest{};
    auto notUsed = Time{};
    using enum opentxs::storage::ErrorReporting;
    const bool haveRequest = api_.Storage().Internal().Load(
        nym,
        requestID,
        otx::client::StorageBox::SENTPEERREQUEST,
        serializedRequest,
        notUsed,
        verbose);

    if (false == haveRequest) {
        LogError()()(
            " The request for this reply does not exist in the sent box.")
            .Flush();

        return false;
    }

    auto serialized = protobuf::PeerReply{};

    if (false == reply.Internal().Serialize(serialized)) {
        LogError()()("Failed to serialize reply.").Flush();

        return false;
    }
    const bool receivedReply = api_.Storage().Internal().Store(
        serialized, nym, otx::client::StorageBox::INCOMINGPEERREPLY);

    if (receivedReply) {
        peer_reply_publisher_->Send([&] {
            auto out = opentxs::network::zeromq::Message{};
            out.AddFrame();
            out.AddFrame(nym);
            out.Internal().AddFrame(serialized);

            return out;
        }());
        peer_reply_new_publisher_->Send([&] {
            auto out = MakeWork(WorkType::PeerReply);
            reply.ID().Serialize(out);
            reply.Responder().Serialize(out);
            reply.Initiator().Serialize(out);
            out.AddFrame(reply.Type());
            out.Internal().AddFrame(serialized);

            return out;
        }());
    } else {
        LogError()()("Failed to save incoming reply.").Flush();

        return false;
    }

    const bool finishedRequest = api_.Storage().Internal().Store(
        serializedRequest, nym, otx::client::StorageBox::FINISHEDPEERREQUEST);

    if (!finishedRequest) {
        LogError()()(" Failed to save request to finished box.").Flush();

        return false;
    }

    const bool removedRequest = api_.Storage().Internal().RemoveNymBoxItem(
        nym, otx::client::StorageBox::SENTPEERREQUEST, requestID);

    if (!finishedRequest) {
        LogError()()(" Failed to delete finished request from sent box.")
            .Flush();
    }

    return removedRequest;
}

auto WalletPrivate::PeerRequest(
    const identifier::Nym& id,
    const identifier::Generic& request,
    const otx::client::StorageBox& box,
    alloc::Strategy alloc) const noexcept -> contract::peer::Request
{
    try {
        auto time = Time{};
        const auto proto = [&] {
            auto lock = Lock{peer_lock(id.asBase58(api_.Crypto()))};
            auto out = protobuf::PeerRequest{};
            using enum opentxs::storage::ErrorReporting;
            const auto loaded = api_.Storage().Internal().Load(
                id, request, box, out, time, silent);

            if (false == loaded) {
                throw std::runtime_error{"reply not found"};
            }

            return out;
        }();
        auto out =
            api_.Factory().Internal().Session().PeerRequest(proto, alloc);
        out.Internal().SetReceived(time);

        return out;
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return {alloc.result_};
    }
}

auto WalletPrivate::PeerRequestComplete(
    const identifier::Nym& nym,
    const identifier::Generic& replyID) const -> bool
{
    const auto nymID = nym.asBase58(api_.Crypto());
    const auto lock = Lock{peer_lock(nymID)};
    auto reply = protobuf::PeerReply{};
    using enum opentxs::storage::ErrorReporting;
    const bool haveReply = api_.Storage().Internal().Load(
        nym,
        replyID,
        otx::client::StorageBox::INCOMINGPEERREPLY,
        reply,
        verbose);

    if (!haveReply) {
        LogError()()(" The reply does not exist in the incoming box.").Flush();

        return false;
    }

    // This reply may have been loaded by request id.
    const auto& realReplyID = api_.Factory().Internal().Identifier(reply.id());
    const bool storedReply = api_.Storage().Internal().Store(
        reply, nym, otx::client::StorageBox::PROCESSEDPEERREPLY);

    if (!storedReply) {
        LogError()()(" Failed to save reply to processed box.").Flush();

        return false;
    }

    const bool removedReply = api_.Storage().Internal().RemoveNymBoxItem(
        nym, otx::client::StorageBox::INCOMINGPEERREPLY, realReplyID);

    if (!removedReply) {
        LogError()()(" Failed to delete completed reply from incoming box.")
            .Flush();
    }

    return removedReply;
}

auto WalletPrivate::PeerRequestCreate(
    const identifier::Nym& nym,
    const protobuf::PeerRequest& request) const -> bool
{
    const auto nymID = nym.asBase58(api_.Crypto());
    const auto lock = Lock{peer_lock(nymID)};

    return api_.Storage().Internal().Store(
        request, nym, otx::client::StorageBox::SENTPEERREQUEST);
}

auto WalletPrivate::PeerRequestCreateRollback(
    const identifier::Nym& nym,
    const identifier::Generic& request) const -> bool
{
    const auto nymID = nym.asBase58(api_.Crypto());
    const auto lock = Lock{peer_lock(nymID)};

    return api_.Storage().Internal().RemoveNymBoxItem(
        nym, otx::client::StorageBox::SENTPEERREQUEST, request);
}

auto WalletPrivate::PeerRequestDelete(
    const identifier::Nym& nym,
    const identifier::Generic& request,
    const otx::client::StorageBox& box) const -> bool
{
    switch (box) {
        case otx::client::StorageBox::SENTPEERREQUEST:
        case otx::client::StorageBox::INCOMINGPEERREQUEST:
        case otx::client::StorageBox::FINISHEDPEERREQUEST:
        case otx::client::StorageBox::PROCESSEDPEERREQUEST: {
            return api_.Storage().Internal().RemoveNymBoxItem(
                nym, box, request);
        }
        default: {
            return false;
        }
    }
}

auto WalletPrivate::PeerRequestSent(const identifier::Nym& nym) const
    -> ObjectList
{
    const auto nymID = nym.asBase58(api_.Crypto());
    const auto lock = Lock{peer_lock(nymID)};

    return api_.Storage().Internal().NymBoxList(
        nym, otx::client::StorageBox::SENTPEERREQUEST);
}

auto WalletPrivate::PeerRequestIncoming(const identifier::Nym& nym) const
    -> ObjectList
{
    const auto nymID = nym.asBase58(api_.Crypto());
    const auto lock = Lock{peer_lock(nymID)};

    return api_.Storage().Internal().NymBoxList(
        nym, otx::client::StorageBox::INCOMINGPEERREQUEST);
}

auto WalletPrivate::PeerRequestFinished(const identifier::Nym& nym) const
    -> ObjectList
{
    const auto nymID = nym.asBase58(api_.Crypto());
    const auto lock = Lock{peer_lock(nymID)};

    return api_.Storage().Internal().NymBoxList(
        nym, otx::client::StorageBox::FINISHEDPEERREQUEST);
}

auto WalletPrivate::PeerRequestProcessed(const identifier::Nym& nym) const
    -> ObjectList
{
    const auto nymID = nym.asBase58(api_.Crypto());
    const auto lock = Lock{peer_lock(nymID)};

    return api_.Storage().Internal().NymBoxList(
        nym, otx::client::StorageBox::PROCESSEDPEERREQUEST);
}

auto WalletPrivate::PeerRequestReceive(
    const identifier::Nym& nym,
    const PeerObject& object) const -> bool
{
    if (contract::peer::ObjectType::Request != object.Type()) {
        LogError()()("This is not a peer request.").Flush();

        return false;
    }

    const auto& request = object.Request();

    if (false == request.IsValid()) {
        LogError()()("Invalid request.").Flush();

        return false;
    }

    auto serialized = protobuf::PeerRequest{};

    if (false == request.Internal().Serialize(serialized)) {
        LogError()()("Failed to serialize request.").Flush();

        return false;
    }

    const auto nymID = nym.asBase58(api_.Crypto());
    const auto saved = [&] {
        const auto lock = Lock{peer_lock(nymID)};

        return api_.Storage().Internal().Store(
            serialized, nym, otx::client::StorageBox::INCOMINGPEERREQUEST);
    }();

    if (saved) {
        peer_request_publisher_->Send([&] {
            auto out = opentxs::network::zeromq::Message{};
            out.AddFrame();
            out.AddFrame(nymID);
            out.Internal().AddFrame(serialized);

            return out;
        }());
        peer_request_new_publisher_->Send([&] {
            auto out = MakeWork(WorkType::PeerRequest);
            request.ID().Serialize(out);
            request.Responder().Serialize(out);
            request.Initiator().Serialize(out);
            out.AddFrame(request.Type());
            out.Internal().AddFrame(serialized);

            return out;
        }());
    }

    return saved;
}

auto WalletPrivate::PeerRequestUpdate(
    const identifier::Nym& nym,
    const identifier::Generic& request,
    const otx::client::StorageBox& box) const -> bool
{
    switch (box) {
        case otx::client::StorageBox::SENTPEERREQUEST:
        case otx::client::StorageBox::INCOMINGPEERREQUEST:
        case otx::client::StorageBox::FINISHEDPEERREQUEST:
        case otx::client::StorageBox::PROCESSEDPEERREQUEST: {
            return api_.Storage().Internal().SetPeerRequestTime(
                nym, request, box);
        }
        default: {
            return false;
        }
    }
}

auto WalletPrivate::process_p2p(
    opentxs::network::zeromq::Message&& msg) const noexcept -> void
{
    const auto body = msg.Payload();

    if (0 == body.size()) { LogAbort()().Abort(); }

    using Job = opentxs::network::otdht::Job;
    const auto type = body[0].as<Job>();

    switch (type) {
        case Job::Response: {
            process_p2p_response(std::move(msg));
        } break;
        case Job::PublishContract: {
            process_p2p_publish_contract(std::move(msg));
        } break;
        case Job::QueryContract: {
            process_p2p_query_contract(std::move(msg));
        } break;
        case Job::Register: {
            batch_.toggle_ = true;
        } break;
        case Job::Shutdown:
        case Job::BlockHeader:
        case Job::Reorg:
        case Job::SyncServerUpdated:
        case Job::SyncAck:
        case Job::SyncReply:
        case Job::SyncPush:
        case Job::Request:
        case Job::Processed:
        case Job::StateMachine:
        default: {
            LogError()()("Unsupported message type on internal socket: ")(
                static_cast<OTZMQWorkType>(type))
                .Flush();

            LogAbort()().Abort();
        }
    }
}

auto WalletPrivate::process_p2p_publish_contract(
    opentxs::network::zeromq::Message&& msg) const noexcept -> void
{
    try {
        const auto base = api_.Factory().BlockchainSyncMessage(msg);

        if (!base) {
            throw std::runtime_error{"failed to instantiate message"};
        }

        using Type = opentxs::network::otdht::MessageType;
        const auto type = base->Type();

        if (Type::publish_contract != type) {
            const auto error = CString{}
                                   .append("Unsupported message type ")
                                   .append(print(type));

            throw std::runtime_error{error.c_str()};
        }

        const auto& contract = base->asPublishContract();
        const auto& id = contract.ID();
        auto payload = [&] {
            const auto ctype = contract.ContractType();
            using enum contract::Type;

            switch (ctype) {
                case nym: {
                    const auto nym = Nym(contract.Payload());

                    return (nym && nym->ID() == id);
                }
                case notary: {
                    const auto notary = Server(contract.Payload());

                    return (notary->ID() == id);
                }
                case unit: {
                    const auto unit = UnitDefinition(contract.Payload());

                    return (unit->ID() == id);
                }
                case invalid:
                default: {
                    const auto error =
                        CString{"unsupported or unknown contract type: "}
                            .append(print(ctype));

                    throw std::runtime_error{error.c_str()};
                }
            }
        }();
        p2p_socket_.Send([&] {
            auto out =
                opentxs::network::zeromq::reply_to_message(std::move(msg));
            const auto reply =
                factory::BlockchainSyncPublishContractReply(id, payload);
            reply.Serialize(out);

            return out;
        }());
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();
    }
}

auto WalletPrivate::process_p2p_query_contract(
    opentxs::network::zeromq::Message&& msg) const noexcept -> void
{
    try {
        const auto base = api_.Factory().BlockchainSyncMessage(msg);

        if (!base) {
            throw std::runtime_error{"failed to instantiate message"};
        }

        using Type = opentxs::network::otdht::MessageType;
        const auto type = base->Type();

        if (Type::contract_query != type) {
            const auto error = CString{}
                                   .append("Unsupported message type ")
                                   .append(print(type));

            throw std::runtime_error{error.c_str()};
        }

        auto payload = [&] {
            const auto& id = base->asQueryContract().ID();
            const auto ctype = translate(id.Type());

            try {
                using enum contract::Type;
                switch (ctype) {
                    case nym: {
                        const auto nymID =
                            api_.Factory().Internal().NymIDConvertSafe(id);
                        const auto nym = Nym(nymID);

                        if (!nym) {
                            throw std::runtime_error{
                                UnallocatedCString{"nym "} +
                                nymID.asBase58(api_.Crypto()) + " not found"};
                        }

                        return factory::BlockchainSyncQueryContractReply(*nym);
                    }
                    case notary: {
                        const auto notaryID =
                            api_.Factory().Internal().NotaryIDConvertSafe(id);

                        return factory::BlockchainSyncQueryContractReply(
                            Server(notaryID));
                    }
                    case unit: {
                        const auto unitID =
                            api_.Factory().Internal().UnitIDConvertSafe(id);

                        return factory::BlockchainSyncQueryContractReply(
                            UnitDefinition(unitID));
                    }
                    case invalid:
                    default: {
                        const auto error =
                            CString{"unsupported or unknown contract type: "}
                                .append(print(type));

                        throw std::runtime_error{error.c_str()};
                    }
                }
            } catch (const std::exception& e) {
                LogError()()(e.what()).Flush();

                return factory::BlockchainSyncQueryContractReply(id);
            }
        }();
        p2p_socket_.Send([&] {
            auto out =
                opentxs::network::zeromq::reply_to_message(std::move(msg));
            payload.Serialize(out);

            return out;
        }());
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();
    }
}

auto WalletPrivate::process_p2p_response(
    opentxs::network::zeromq::Message&& msg) const noexcept -> void
{
    try {
        const auto base = api_.Factory().BlockchainSyncMessage(msg);

        if (!base) {
            throw std::runtime_error{"failed to instantiate message"};
        }

        using Type = opentxs::network::otdht::MessageType;
        const auto type = base->Type();

        switch (type) {
            case Type::publish_ack: {
                const auto& contract = base->asPublishContractReply();
                const auto& id = contract.ID();
                const auto& log = LogVerbose();
                log("Contract ")(id, api_.Crypto())(" ");

                if (contract.Success()) {
                    log("successfully");
                } else {
                    log("not");
                }

                log(" published").Flush();
            } break;
            case Type::contract: {
                const auto& contract = base->asQueryContractReply();
                const auto& id = contract.ID();
                const auto& log = LogVerbose();
                const auto success = [&] {
                    const auto ctype = contract.ContractType();
                    using enum contract::Type;

                    switch (ctype) {
                        case nym: {
                            log("Nym");

                            if (!valid(contract.Payload())) { return false; }

                            const auto nym = Nym(contract.Payload());

                            return (nym && nym->ID() == id);
                        }
                        case notary: {
                            log("Notary contract");

                            if (!valid(contract.Payload())) { return false; }

                            const auto notary = Server(contract.Payload());

                            return (notary->ID() == id);
                        }
                        case unit: {
                            log("Unit definition");

                            if (!valid(contract.Payload())) { return false; }

                            const auto unit =
                                UnitDefinition(contract.Payload());

                            return (unit->ID() == id);
                        }
                        case invalid:
                        default: {
                            const auto error =
                                CString{
                                    "unsupported or unknown contract type: "}
                                    .append(print(ctype));

                            throw std::runtime_error{error.c_str()};
                        }
                    }
                }();
                log(" ")(id, api_.Crypto())(" ");

                if (success) {
                    log("successfully retrieved");
                } else {
                    log("not found on remote node");
                }

                log.Flush();
            } break;
            default:
                const auto error = CString{}
                                       .append("Unsupported message type ")
                                       .append(print(type));

                throw std::runtime_error{error.c_str()};
        }
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();
    }
}

auto WalletPrivate::PublishNotary(const identifier::Notary& id) const noexcept
    -> bool
{
    try {
        to_loopback_.modify_detach([notary = Server(id)](auto& socket) {
            const auto command = factory::BlockchainSyncPublishContract(notary);
            socket.Send([&] {
                auto out = opentxs::network::zeromq::Message{};
                command.Serialize(out);

                return out;
            }());
        });

        return true;
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return false;
    }
}

auto WalletPrivate::PublishNym(const identifier::Nym& id) const noexcept -> bool
{
    auto nym = Nym(id);

    if (!nym) {
        LogError()()("nym ")(id, api_.Crypto())(" does not exist").Flush();

        return false;
    }

    to_loopback_.modify_detach([n = std::move(nym)](auto& socket) {
        const auto command = factory::BlockchainSyncPublishContract(*n);
        socket.Send([&] {
            auto out = opentxs::network::zeromq::Message{};
            command.Serialize(out);

            return out;
        }());
    });

    return true;
}

auto WalletPrivate::PublishUnit(
    const identifier::UnitDefinition& id) const noexcept -> bool
{
    try {
        to_loopback_.modify_detach([unit = UnitDefinition(id)](auto& socket) {
            const auto command = factory::BlockchainSyncPublishContract(unit);
            socket.Send([&] {
                auto out = opentxs::network::zeromq::Message{};
                command.Serialize(out);

                return out;
            }());
        });

        return true;
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return false;
    }
}

auto WalletPrivate::purse(
    const identifier::Nym& nym,
    const identifier::Notary& server,
    const identifier::UnitDefinition& unit,
    ErrorReporting checking) const -> PurseMap::mapped_type&
{
    const auto id = PurseID{nym, server, unit};
    auto lock = Lock{purse_lock_};
    auto& out = purse_map_[id];
    auto& [mutex, purse] = out;

    if (purse) { return out; }

    auto serialized = protobuf::Purse{};
    const auto loaded =
        api_.Storage().Internal().Load(nym, server, unit, serialized, checking);

    if (false == loaded) {
        using enum ErrorReporting;

        if (verbose == checking) {
            LogError()()("Purse does not exist").Flush();
        }

        return out;
    }

    if (false == protobuf::syntax::check(LogError(), serialized)) {
        LogError()()("Invalid purse").Flush();

        return out;
    }

    purse = factory::Purse(api_, serialized);

    if (false == bool(purse)) {
        LogError()()("Failed to instantiate purse").Flush();
    }

    return out;
}

auto WalletPrivate::Purse(
    const identifier::Nym& nym,
    const identifier::Notary& server,
    const identifier::UnitDefinition& unit,
    bool checking) const -> const otx::blind::Purse&
{
    return purse(nym, server, unit, static_cast<ErrorReporting>(checking))
        .second;
}

auto WalletPrivate::mutable_Purse(
    const identifier::Nym& nymID,
    const identifier::Notary& server,
    const identifier::UnitDefinition& unit,
    const PasswordPrompt& reason,
    const otx::blind::CashType type) const
    -> Editor<otx::blind::Purse, std::shared_mutex>
{
    using enum ErrorReporting;
    auto& [mutex, purse] = this->purse(nymID, server, unit, silent);

    if (!purse) {
        const auto nym = Nym(nymID);

        assert_false(nullptr == nym);

        purse = factory::Purse(api_, *nym, server, unit, type, reason);
    }

    assert_true(purse);

    return {
        mutex,
        &purse,
        [this, nym = identifier::Nym{nymID}](
            auto* in, const auto& lock) -> void { this->save(lock, nym, in); }};
}

auto WalletPrivate::RemoveServer(const identifier::Notary& id) const -> bool
{
    const auto mapLock = Lock{server_map_lock_};
    auto deleted = server_map_.erase(id);

    if (0 != deleted) { return api_.Storage().Internal().RemoveServer(id); }

    return false;
}

auto WalletPrivate::RemoveUnitDefinition(
    const identifier::UnitDefinition& id) const -> bool
{
    const auto mapLock = Lock{unit_map_lock_};
    auto deleted = unit_map_.erase(id);

    if (0 != deleted) {
        return api_.Storage().Internal().RemoveUnitDefinition(id);
    }

    return false;
}

auto WalletPrivate::publish_server(const identifier::Notary& id) const noexcept
    -> void
{
    server_publisher_->Send([&] {
        auto work = opentxs::network::zeromq::tagged_message(
            WorkType::NotaryUpdated, true);
        work.AddFrame(id);

        return work;
    }());
}

auto WalletPrivate::publish_unit(
    const identifier::UnitDefinition& id) const noexcept -> void
{
    unit_publisher_->Send([&] {
        auto work = opentxs::network::zeromq::tagged_message(
            WorkType::UnitDefinitionUpdated, true);
        work.AddFrame(id);

        return work;
    }());
}

auto WalletPrivate::reverse_unit_map(const UnitNameMap& map)
    -> WalletPrivate::UnitNameReverse
{
    UnitNameReverse output{};

    for (const auto& [key, value] : map) { output.emplace(value, key); }

    return output;
}

void WalletPrivate::save(
    const PasswordPrompt& reason,
    const identifier::Account& id,
    std::unique_ptr<opentxs::Account>& in,
    eLock&,
    bool success) const
{
    assert_false(nullptr == in);

    auto& account = *in;

    if (false == success) {
        // Reload the last valid state for this Account.
        UnallocatedCString serialized{""};
        UnallocatedCString alias{""};
        using enum opentxs::storage::ErrorReporting;
        const auto loaded =
            api_.Storage().Internal().Load(id, serialized, alias, verbose);

        assert_true(loaded);

        in.reset(account_factory(id, alias, serialized));

        assert_false(nullptr == in);

        return;
    }

    const auto signerID = api_.Storage().Internal().AccountSigner(id);

    assert_false(signerID.empty());

    const auto signerNym = Nym(signerID);

    assert_false(nullptr == signerNym);

    account.ReleaseSignatures();
    auto saved = account.SignContract(*signerNym, reason);

    assert_true(saved);

    saved = account.SaveContract();

    assert_true(saved);

    auto serialized = String::Factory();
    saved = in->SaveContractRaw(serialized);

    assert_true(saved);

    const auto contractID = api_.Storage().Internal().AccountContract(id);

    assert_false(contractID.empty());

    saved = api_.Storage().Internal().Store(
        id,
        serialized->Get(),
        in->Alias(),
        api_.Storage().Internal().AccountOwner(id),
        api_.Storage().Internal().AccountSigner(id),
        api_.Storage().Internal().AccountIssuer(id),
        api_.Storage().Internal().AccountServer(id),
        contractID,
        extract_unit(contractID));

    assert_true(saved);
}

void WalletPrivate::save(
    const PasswordPrompt& reason,
    otx::context::internal::Base* context) const
{
    if (nullptr == context) { return; }

    const auto saved = context->Save(reason);

    assert_true(saved);
}

void WalletPrivate::save(const Lock& lock, otx::client::Issuer* in) const
{
    assert_false(nullptr == in);
    assert_true(lock.owns_lock());

    const auto& nymID = in->LocalNymID();
    const auto& issuerID = in->IssuerID();
    auto serialized = protobuf::Issuer{};
    auto loaded = in->Serialize(serialized);

    assert_true(loaded);

    api_.Storage().Internal().Store(nymID, serialized);
    issuer_publisher_->Send([&] {
        auto work = opentxs::network::zeromq::tagged_message(
            WorkType::IssuerUpdated, true);
        work.AddFrame(nymID);
        work.AddFrame(issuerID);

        return work;
    }());
}

void WalletPrivate::save(
    const eLock& lock,
    const identifier::Nym nym,
    otx::blind::Purse* in) const
{
    assert_false(nullptr == in);
    assert_true(lock.owns_lock());

    auto& purse = *in;

    if (!purse) { LogAbort()().Abort(); }

    const auto serialized = [&] {
        auto proto = protobuf::Purse{};
        purse.Internal().Serialize(proto);

        return proto;
    }();

    assert_true(protobuf::syntax::check(LogError(), serialized));

    const auto stored = api_.Storage().Internal().Store(nym, serialized);

    assert_true(stored);
}

void WalletPrivate::save(NymData* nymData, const Lock& lock) const
{
    assert_false(nullptr == nymData);
    assert_true(lock.owns_lock());

    SaveCredentialIDs(nymData->nym());
    notify_changed(nymData->nym().ID());
}

void WalletPrivate::save(
    const PasswordPrompt& reason,
    opentxs::NymFile* nymfile,
    const Lock& lock) const
{
    assert_false(nullptr == nymfile);
    assert_true(lock.owns_lock());

    auto* internal = dynamic_cast<opentxs::internal::NymFile*>(nymfile);

    assert_false(nullptr == internal);

    const auto saved = internal->SaveSignedNymFile(reason);

    assert_true(saved);
}

auto WalletPrivate::SaveCredentialIDs(const identity::Nym& nym) const -> bool
{
    auto index = protobuf::Nym{};
    if (false == dynamic_cast<const identity::internal::Nym&>(nym)
                     .SerializeCredentialIndex(
                         index, identity::internal::Nym::Mode::Abbreviated)) {
        return false;
    }
    const auto valid = protobuf::syntax::check(LogError(), index);

    if (!valid) { return false; }

    if (!api_.Storage().Internal().Store(index, nym.Alias())) {
        LogError()()("Failure trying to store credential list for Nym: ")(
            nym.ID(), api_.Crypto())
            .Flush();

        return false;
    }

    LogDetail()()("Credentials saved.").Flush();

    return true;
}

auto WalletPrivate::search_notary(const identifier::Notary& id) const noexcept
    -> void
{
    LogVerbose()()("Searching remote networks for unknown notary ")(
        id, api_.Crypto())
        .Flush();
    to_loopback_.modify_detach([id](auto& socket) {
        const auto command = factory::BlockchainSyncQueryContract(id);
        socket.Send([&] {
            auto out = opentxs::network::zeromq::Message{};
            command.Serialize(out);

            return out;
        }());
    });
}

auto WalletPrivate::search_nym(const identifier::Nym& id) const noexcept -> void
{
    LogVerbose()()("Searching remote networks for unknown nym ")(
        id, api_.Crypto())
        .Flush();
    to_loopback_.modify_detach([id](auto& socket) {
        const auto command = factory::BlockchainSyncQueryContract(id);
        socket.Send([&] {
            auto out = opentxs::network::zeromq::Message{};
            command.Serialize(out);

            return out;
        }());
    });
}

auto WalletPrivate::search_unit(
    const identifier::UnitDefinition& id) const noexcept -> void
{
    LogVerbose()()("Searching remote networks for unknown unit definition ")(
        id, api_.Crypto())
        .Flush();
    to_loopback_.modify_detach([id](auto& socket) {
        const auto command = factory::BlockchainSyncQueryContract(id);
        socket.Send([&] {
            auto out = opentxs::network::zeromq::Message{};
            command.Serialize(out);

            return out;
        }());
    });
}

auto WalletPrivate::SetDefaultNym(const identifier::Nym& id) const noexcept
    -> bool
{
    if (id.empty()) {
        LogError()()("Invalid id").Flush();

        return false;
    }

    if (false == LocalNyms().contains(id)) {
        LogError()()("Nym ")(id, api_.Crypto())(" is not local").Flush();

        return false;
    }

    auto out = api_.Storage().Internal().SetDefaultNym(id);

    if (out) { notify_changed(id); }

    return out;
}

auto WalletPrivate::SetNymAlias(
    const identifier::Nym& id,
    std::string_view alias) const -> bool
{
    auto mapLock = Lock{nym_map_lock_};
    auto& nym = nym_map_[id].second;
    if (nullptr != nym) { nym->SetAlias(alias); }

    return api_.Storage().Internal().SetNymAlias(id, alias);
}

auto WalletPrivate::Server(
    const identifier::Notary& id,
    const std::chrono::milliseconds& timeout) const -> OTServerContract
{
    if (blockchain::Type::UnknownBlockchain != blockchain::Chain(api_, id)) {
        throw std::runtime_error{"Attempting to load a blockchain as a notary"};
    }

    if (id.empty()) {
        throw std::runtime_error{"Attempting to load a null notary contract"};
    }

    auto mapLock = Lock{server_map_lock_};
    const bool inMap = (server_map_.find(id) != server_map_.end());
    bool valid = false;

    if (!inMap) {
        auto serialized = protobuf::ServerContract{};
        auto alias = UnallocatedCString{};
        using enum opentxs::storage::ErrorReporting;
        const bool loaded =
            api_.Storage().Internal().Load(id, serialized, alias, silent);

        if (loaded) {
            auto nym = Nym(api_.Factory().Internal().NymID(serialized.nymid()));

            if (!nym && serialized.has_publicnym()) {
                nym = Nym(serialized.publicnym());
            }

            if (nym) {
                auto& pServer = server_map_[id];
                pServer =
                    opentxs::Factory::ServerContract(api_, nym, serialized);

                if (pServer) {
                    valid = true;  // Factory() performs validation
                    pServer->InitAlias(alias);
                } else {
                    server_map_.erase(id);
                }
            }
        } else {
            search_notary(id);

            if (timeout > 0ms) {
                mapLock.unlock();
                auto start = std::chrono::high_resolution_clock::now();
                auto end = start + timeout;
                const auto interval = 100ms;

                while (std::chrono::high_resolution_clock::now() < end) {
                    std::this_thread::sleep_for(interval);
                    mapLock.lock();
                    const bool found =
                        (server_map_.find(id) != server_map_.end());
                    mapLock.unlock();

                    if (found) { break; }
                }

                return Server(id);  // timeout of zero prevents infinite
                                    // recursion
            }
        }
    } else {
        auto& pServer = server_map_[id];
        if (pServer) { valid = pServer->Validate(); }
    }

    if (valid) { return OTServerContract{server_map_[id]}; }

    throw std::runtime_error("Server contract not found");
}

auto WalletPrivate::server(std::unique_ptr<contract::Server> contract) const
    noexcept(false) -> OTServerContract
{
    if (false == bool(contract)) {
        throw std::runtime_error("Null server contract");
    }

    if (false == contract->Validate()) {
        throw std::runtime_error("Invalid server contract");
    }

    const auto id = [&] {
        const auto generic = contract->ID();
        auto output = identifier::Notary{};
        output.Assign(generic);

        return output;
    }();

    assert_false(id.empty());
    assert_true(contract->Alias() == contract->EffectiveName());

    auto serialized = protobuf::ServerContract{};

    if (false == contract->Serialize(serialized)) {
        LogError()()("Failed to serialize contract.").Flush();
    }

    if (api_.Storage().Internal().Store(serialized, contract->Alias())) {
        {
            const auto mapLock = Lock{server_map_lock_};
            server_map_[id].reset(contract.release());
        }

        publish_server(id);
    } else {
        LogError()()("Failed to save server contract.").Flush();
    }

    return Server(id);
}

auto WalletPrivate::Server(const protobuf::ServerContract& contract) const
    -> OTServerContract
{
    if (false == protobuf::syntax::check(LogError(), contract)) {
        throw std::runtime_error("Invalid serialized server contract");
    }

    const auto serverID = api_.Factory().Internal().NotaryID(contract.id());

    if (serverID.empty()) {
        throw std::runtime_error(
            "Attempting to load notary contract with empty notary ID");
    }

    const auto nymID = api_.Factory().Internal().NymID(contract.nymid());

    if (nymID.empty()) {
        throw std::runtime_error(
            "Attempting to load notary contract with empty nym ID");
    }

    find_nym_->Send([&] {
        auto work = opentxs::network::zeromq::tagged_message(
            WorkType::OTXSearchNym, true);
        work.AddFrame(nymID);

        return work;
    }());
    auto nym = Nym(nymID);

    if (!nym && contract.has_publicnym()) { nym = Nym(contract.publicnym()); }

    if (!nym) { throw std::runtime_error("Unable to load notary nym"); }

    auto candidate = std::unique_ptr<contract::Server>{
        opentxs::Factory::ServerContract(api_, nym, contract)};

    if (!candidate) {
        throw std::runtime_error("Failed to instantiate contract");
    }

    if (false == candidate->Validate()) {
        throw std::runtime_error("Invalid contract");
    }

    if (serverID != candidate->ID()) {
        throw std::runtime_error("Wrong contract ID");
    }

    auto serialized = protobuf::ServerContract{};

    if (false == candidate->Serialize(serialized)) {
        throw std::runtime_error("Failed to serialize server contract");
    }

    const auto stored =
        api_.Storage().Internal().Store(serialized, candidate->EffectiveName());

    if (false == stored) {
        throw std::runtime_error("Failed to save server contract");
    }

    {
        const auto mapLock = Lock{server_map_lock_};
        server_map_[serverID].reset(candidate.release());
    }

    publish_server(serverID);

    return Server(serverID);
}

auto WalletPrivate::Server(const ReadView& contract) const -> OTServerContract
{
    return Server(
        opentxs::protobuf::Factory<protobuf::ServerContract>(contract));
}

auto WalletPrivate::Server(
    const UnallocatedCString& nymid,
    const UnallocatedCString& name,
    const UnallocatedCString& terms,
    const UnallocatedList<contract::Server::Endpoint>& endpoints,
    const opentxs::PasswordPrompt& reason,
    const VersionNumber version) const -> OTServerContract
{
    auto nym = Nym(api_.Factory().NymIDFromBase58(nymid));

    if (nym) {
        auto list = UnallocatedList<Endpoint>{};
        std::ranges::transform(
            endpoints,
            std::back_inserter(list),
            [](const auto& in) -> Endpoint {
                return {
                    static_cast<int>(std::get<0>(in)),
                    static_cast<int>(std::get<1>(in)),
                    std::get<2>(in),
                    std::get<3>(in),
                    std::get<4>(in)};
            });
        auto pContract =
            std::unique_ptr<contract::Server>{opentxs::Factory::ServerContract(
                api_, nym, list, terms, name, version, reason)};

        if (pContract) {

            return this->server(std::move(pContract));
        } else {
            LogError()()(" Error: Failed to create contract.").Flush();
        }
    } else {
        LogError()()("Error: Nym does not exist.").Flush();
    }

    return Server(identifier::Notary{});
}

auto WalletPrivate::ServerList() const -> ObjectList
{
    return api_.Storage().ServerList();
}

auto WalletPrivate::server_to_nym(identifier::Generic& input) const
    -> identifier::Nym
{
    auto output = identifier::Nym{};
    output.Assign(input);
    const auto inputIsNymID = [&] {
        auto nym = Nym(output);

        return nym.operator bool();
    }();

    if (inputIsNymID) {
        const auto list = ServerList();
        std::size_t matches = 0;

        for (const auto& [serverID, alias] : list) {
            try {
                const auto id = api_.Factory().NotaryIDFromBase58(serverID);
                auto server = Server(id);

                if (server->Signer()->ID() == input) {
                    matches++;
                    // set input to the notary ID
                    input.Assign(server->ID());
                }
            } catch (...) {
            }
        }

        assert_true(2 > matches);
    } else {
        output.clear();

        try {
            const auto notaryID = [&] {
                auto out = identifier::Notary{};
                out.Assign(input);

                return out;
            }();
            const auto contract = Server(notaryID);
            output = contract->Signer()->ID();
        } catch (...) {
            LogDetail()()("Non-existent server: ")(input, api_.Crypto())
                .Flush();
        }
    }

    return output;
}

auto WalletPrivate::SetServerAlias(
    const identifier::Notary& id,
    std::string_view alias) const -> bool
{
    const bool saved = api_.Storage().Internal().SetServerAlias(id, alias);

    if (saved) {
        {
            const auto mapLock = Lock{server_map_lock_};
            server_map_.erase(id);
        }

        publish_server(id);

        return true;
    } else {
        LogError()()("Failed to save server contract ")(id, api_.Crypto())
            .Flush();
    }

    return false;
}

auto WalletPrivate::SetUnitDefinitionAlias(
    const identifier::UnitDefinition& id,
    std::string_view alias) const -> bool
{
    const bool saved =
        api_.Storage().Internal().SetUnitDefinitionAlias(id, alias);

    if (saved) {
        {
            const auto mapLock = Lock{unit_map_lock_};
            unit_map_.erase(id);
        }

        publish_unit(id);

        return true;
    } else {
        LogError()()("Failed to save unit definition ")(id, api_.Crypto())
            .Flush();
    }

    return false;
}

auto WalletPrivate::UnitDefinitionList() const -> ObjectList
{
    return api_.Storage().UnitDefinitionList();
}

auto WalletPrivate::UnitDefinition(
    const identifier::UnitDefinition& id,
    const std::chrono::milliseconds& timeout) const -> OTUnitDefinition
{
    if (blockchain::Type::UnknownBlockchain != blockchain::Chain(api_, id)) {
        throw std::runtime_error{
            "Attempting to load a blockchain as a unit definition"};
    }

    if (id.empty()) {
        throw std::runtime_error{"Attempting to load a null unit definition"};
    }

    auto mapLock = Lock{unit_map_lock_};
    const bool inMap = (unit_map_.find(id) != unit_map_.end());
    bool valid = false;

    if (!inMap) {
        auto serialized = protobuf::UnitDefinition{};
        UnallocatedCString alias;
        using enum opentxs::storage::ErrorReporting;
        const bool loaded =
            api_.Storage().Internal().Load(id, serialized, alias, silent);

        if (loaded) {
            auto nym =
                Nym(api_.Factory().Internal().NymID(serialized.issuer()));

            if (!nym && serialized.has_issuer_nym()) {
                nym = Nym(serialized.issuer_nym());
            }

            if (nym) {
                auto& pUnit = unit_map_[id];
                pUnit = opentxs::Factory::UnitDefinition(api_, nym, serialized);

                if (pUnit) {
                    valid = true;  // Factory() performs validation
                    pUnit->InitAlias(alias);
                } else {
                    unit_map_.erase(id);
                }
            }
        } else {
            search_unit(id);

            if (timeout > 0ms) {
                mapLock.unlock();
                auto start = std::chrono::high_resolution_clock::now();
                auto end = start + timeout;
                const auto interval = 100ms;

                while (std::chrono::high_resolution_clock::now() < end) {
                    std::this_thread::sleep_for(interval);
                    mapLock.lock();
                    const bool found = (unit_map_.find(id) != unit_map_.end());
                    mapLock.unlock();

                    if (found) { break; }
                }

                return UnitDefinition(id);  // timeout of zero prevents infinite
                                            // recursion
            }
        }
    } else {
        auto& pUnit = unit_map_[id];
        if (pUnit) { valid = pUnit->Validate(); }
    }

    if (valid) { return OTUnitDefinition{unit_map_[id]}; }

    throw std::runtime_error("Unit definition does not exist");
}

auto WalletPrivate::unit_definition(
    std::shared_ptr<contract::Unit>&& contract) const -> OTUnitDefinition
{
    if (false == bool(contract)) {
        throw std::runtime_error("Null unit definition contract");
    }

    if (false == contract->Validate()) {
        throw std::runtime_error("Invalid unit definition contract");
    }

    const auto id = [&] {
        auto out = identifier::UnitDefinition{};
        out.Assign(contract->ID());

        return out;
    }();

    auto serialized = protobuf::UnitDefinition{};

    if (false == contract->Serialize(serialized)) {
        LogError()()("Failed to serialize unit definition").Flush();
    }

    if (api_.Storage().Internal().Store(serialized, contract->Alias())) {
        {
            const auto mapLock = Lock{unit_map_lock_};
            auto it = unit_map_.find(id);

            if (unit_map_.end() == it) {
                unit_map_.emplace(id, std::move(contract));
            } else {
                it->second = std::move(contract);
            }
        }

        publish_unit(id);
    } else {
        LogError()()("Failed to save unit definition").Flush();
    }

    return UnitDefinition(id);
}

auto WalletPrivate::UnitDefinition(
    const protobuf::UnitDefinition& contract) const -> OTUnitDefinition
{
    if (false == protobuf::syntax::check(LogError(), contract)) {
        throw std::runtime_error("Invalid serialized unit definition");
    }

    const auto unitID = api_.Factory().Internal().UnitID(contract.id());

    if (unitID.empty()) {
        throw std::runtime_error("Invalid unit definition id");
    }

    const auto nymID = api_.Factory().Internal().NymID(contract.issuer());

    if (nymID.empty()) { throw std::runtime_error("Invalid nym ID"); }

    find_nym_->Send([&] {
        auto work = opentxs::network::zeromq::tagged_message(
            WorkType::OTXSearchNym, true);
        work.AddFrame(nymID);

        return work;
    }());
    auto nym = Nym(nymID);

    if (!nym && contract.has_issuer_nym()) { nym = Nym(contract.issuer_nym()); }

    if (!nym) { throw std::runtime_error("Invalid nym"); }

    auto candidate = opentxs::Factory::UnitDefinition(api_, nym, contract);

    if (!candidate) {
        throw std::runtime_error("Failed to instantiate contract");
    }

    if (false == candidate->Validate()) {
        throw std::runtime_error("Invalid contract");
    }

    if (unitID != candidate->ID()) {
        throw std::runtime_error("Wrong contract ID");
    }

    auto serialized = protobuf::UnitDefinition{};

    if (false == candidate->Serialize(serialized)) {
        throw std::runtime_error("Failed to serialize unit definition");
    }

    const auto stored =
        api_.Storage().Internal().Store(serialized, candidate->Alias());

    if (false == stored) {
        throw std::runtime_error("Failed to save unit definition");
    }

    {
        const auto mapLock = Lock{unit_map_lock_};
        unit_map_[unitID] = candidate;
    }

    publish_unit(unitID);

    return UnitDefinition(unitID);
}

auto WalletPrivate::UnitDefinition(const ReadView contract) const
    -> OTUnitDefinition
{
    return UnitDefinition(
        opentxs::protobuf::Factory<protobuf::UnitDefinition>(contract));
}

auto WalletPrivate::CurrencyContract(
    const UnallocatedCString& nymid,
    const UnallocatedCString& shortname,
    const UnallocatedCString& terms,
    const UnitType unitOfAccount,
    const Amount& redemptionIncrement,
    const PasswordPrompt& reason) const -> OTUnitDefinition
{
    return CurrencyContract(
        nymid,
        shortname,
        terms,
        unitOfAccount,
        redemptionIncrement,
        display::GetDefinition(unitOfAccount),
        contract::Unit::DefaultVersion,
        reason);
}

auto WalletPrivate::CurrencyContract(
    const UnallocatedCString& nymid,
    const UnallocatedCString& shortname,
    const UnallocatedCString& terms,
    const UnitType unitOfAccount,
    const Amount& redemptionIncrement,
    const display::Definition& displayDefinition,
    const PasswordPrompt& reason) const -> OTUnitDefinition
{
    return CurrencyContract(
        nymid,
        shortname,
        terms,
        unitOfAccount,
        redemptionIncrement,
        displayDefinition,
        contract::Unit::DefaultVersion,
        reason);
}

auto WalletPrivate::CurrencyContract(
    const UnallocatedCString& nymid,
    const UnallocatedCString& shortname,
    const UnallocatedCString& terms,
    const UnitType unitOfAccount,
    const Amount& redemptionIncrement,
    const VersionNumber version,
    const PasswordPrompt& reason) const -> OTUnitDefinition
{
    return CurrencyContract(
        nymid,
        shortname,
        terms,
        unitOfAccount,
        redemptionIncrement,
        display::GetDefinition(unitOfAccount),
        version,
        reason);
}

auto WalletPrivate::CurrencyContract(
    const UnallocatedCString& nymid,
    const UnallocatedCString& shortname,
    const UnallocatedCString& terms,
    const UnitType unitOfAccount,
    const Amount& redemptionIncrement,
    const display::Definition& displayDefinition,
    const VersionNumber version,
    const PasswordPrompt& reason) const -> OTUnitDefinition
{
    auto unit = UnallocatedCString{};
    auto nym = Nym(api_.Factory().NymIDFromBase58(nymid));

    if (nym) {
        auto contract = opentxs::Factory::CurrencyContract(
            api_,
            nym,
            shortname,
            terms,
            unitOfAccount,
            version,
            reason,
            displayDefinition,
            redemptionIncrement);

        if (contract) {

            return unit_definition(std::move(contract));
        } else {
            LogError()()(" Error: Failed to create contract.").Flush();
        }
    } else {
        LogError()()("Error: Nym does not exist.").Flush();
    }

    return UnitDefinition(api_.Factory().UnitIDFromBase58(unit));
}

auto WalletPrivate::SecurityContract(
    const UnallocatedCString& nymid,
    const UnallocatedCString& shortname,
    const UnallocatedCString& terms,
    const UnitType unitOfAccount,
    const PasswordPrompt& reason,
    const display::Definition& displayDefinition,
    const Amount& redemptionIncrement,
    const VersionNumber version) const -> OTUnitDefinition
{
    const UnallocatedCString unit;
    auto nym = Nym(api_.Factory().NymIDFromBase58(nymid));

    if (nym) {
        auto contract = opentxs::Factory::SecurityContract(
            api_,
            nym,
            shortname,
            terms,
            unitOfAccount,
            version,
            reason,
            displayDefinition,
            redemptionIncrement);

        if (contract) {

            return unit_definition(std::move(contract));
        } else {
            LogError()()(" Error: Failed to create contract.").Flush();
        }
    } else {
        LogError()()("Error: Nym does not exist.").Flush();
    }

    return UnitDefinition(api_.Factory().UnitIDFromBase58(unit));
}

auto WalletPrivate::LoadCredential(
    const identifier::Generic& id,
    std::shared_ptr<protobuf::Credential>& credential) const -> bool
{
    if (false == bool(credential)) {
        credential = std::make_shared<protobuf::Credential>();
    }

    assert_false(nullptr == credential);

    return api_.Storage().Internal().Load(id, *credential);
}

auto WalletPrivate::SaveCredential(const protobuf::Credential& credential) const
    -> bool
{
    return api_.Storage().Internal().Store(credential);
}

WalletPrivate::~WalletPrivate()
{
    handle_.Release();
    Detach(self_);
}
}  // namespace opentxs::api::session
