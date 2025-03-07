// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "internal/otx/smartcontract/OTParty.hpp"  // IWYU pragma: associated

#include <cstdint>
#include <memory>
#include <utility>

#include "internal/core/Armored.hpp"
#include "internal/core/String.hpp"
#include "internal/otx/common/Account.hpp"
#include "internal/otx/common/NumList.hpp"
#include "internal/otx/common/script/OTScriptable.hpp"
#include "internal/otx/common/util/Common.hpp"
#include "internal/otx/common/util/Tag.hpp"
#include "internal/otx/consensus/Server.hpp"
#include "internal/otx/smartcontract/OTAgent.hpp"
#include "internal/otx/smartcontract/OTPartyAccount.hpp"
#include "internal/otx/smartcontract/OTSmartContract.hpp"
#include "internal/util/Pimpl.hpp"
#include "internal/util/Shared.hpp"
#include "opentxs/api/Session.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/identifier/Nym.hpp"
#include "opentxs/identity/Nym.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"

namespace opentxs
{
OTParty::OTParty(const api::Session& api, const UnallocatedCString& dataFolder)
    : api_(api)
    , data_folder_{dataFolder}
    , party_name_(nullptr)
    , party_is_nym_(false)
    , owner_id_()
    , authorizing_agent_()
    , agents_()
    , party_accounts_()
    , opening_trans_no_(0)
    , my_signed_copy_(String::Factory())
    , owner_agreement_(nullptr)
{
}

OTParty::OTParty(
    const api::Session& api,
    const UnallocatedCString& dataFolder,
    const char* szName,
    bool bIsOwnerNym,
    const char* szOwnerID,
    const char* szAuthAgent,
    bool bCreateAgent)
    : api_(api)
    , data_folder_{dataFolder}
    , party_name_(nullptr)
    , party_is_nym_(bIsOwnerNym)
    , owner_id_(szOwnerID != nullptr ? szOwnerID : "")
    , authorizing_agent_(szAuthAgent != nullptr ? szAuthAgent : "")
    , agents_()
    , party_accounts_()
    , opening_trans_no_(0)
    , my_signed_copy_(String::Factory())
    , owner_agreement_(nullptr)
{
    party_name_ = new UnallocatedCString(szName != nullptr ? szName : "");

    if (bCreateAgent) {
        const auto strName = String::Factory(authorizing_agent_.c_str()),
                   strNymID = String::Factory(""),
                   strRoleID = String::Factory(""),
                   strGroupName = String::Factory("");

        auto* pAgent = new OTAgent(
            api_,
            true /*bNymRepresentsSelf*/,
            true /*bIsAnIndividual*/,
            strName,
            strNymID,
            strRoleID,
            strGroupName);
        assert_false(nullptr == pAgent);

        if (!AddAgent(*pAgent)) {
            LogError()()("*** Failed *** while adding default "
                         "agent in CONSTRUCTOR! 2.")
                .Flush();
            delete pAgent;
            pAgent = nullptr;
        }
    }
}

OTParty::OTParty(
    const api::Session& api,
    const UnallocatedCString& dataFolder,
    UnallocatedCString str_PartyName,
    const identity::Nym& theNym,  // Nym is BOTH owner AND agent, when using
                                  // this constructor.
    const UnallocatedCString str_agent_name,
    Account* pAccount,
    const UnallocatedCString* pstr_account_name,
    std::int64_t lClosingTransNo)
    : api_(api)
    , data_folder_{dataFolder}
    , party_name_(new UnallocatedCString(str_PartyName))
    , party_is_nym_(true)
    , owner_id_()
    , authorizing_agent_()
    , agents_()
    , party_accounts_()
    , opening_trans_no_(0)
    , my_signed_copy_(String::Factory())
    , owner_agreement_(nullptr)
{
    //  party_name_ = new UnallocatedCString(str_PartyName);
    assert_false(nullptr == party_name_);

    // theNym is owner, therefore save his ID information, and create the agent
    // for this Nym automatically (that's why it was passed in.)
    // This code won't compile until you do.  :-)

    auto strNymID = String::Factory();
    theNym.GetIdentifier(strNymID);
    owner_id_ = strNymID->Get();

    auto* pAgent = new OTAgent(
        api_,
        str_agent_name,
        theNym);  // (The third arg,
                  // bRepresentsSelf,
                  // defaults here to true.)
    assert_false(nullptr == pAgent);

    if (!AddAgent(*pAgent)) {
        LogError()()("*** Failed *** while adding default agent "
                     "in CONSTRUCTOR!")
            .Flush();
        delete pAgent;
        pAgent = nullptr;
    } else {
        authorizing_agent_ = str_agent_name;
    }

    // if pAccount is NOT nullptr, then an account was passed in, so
    // let's also create a default partyaccount for it.
    //
    if (nullptr != pAccount) {
        assert_false(nullptr == pstr_account_name);  // If passing an account,
                                                     // then you MUST pass an
                                                     // account name also.

        const bool bAdded = AddAccount(
            String::Factory(str_agent_name.c_str()),
            pstr_account_name->c_str(),
            *pAccount,
            lClosingTransNo);

        if (!bAdded) {
            LogError()()("*** Failed *** while adding default "
                         "account in CONSTRUCTOR!")
                .Flush();
        }
    }
}

// Checks opening number on party, and closing numbers on his accounts.
auto OTParty::HasTransactionNum(const std::int64_t& lInput) const -> bool
{
    if (lInput == opening_trans_no_) { return true; }

    for (const auto& it : party_accounts_) {
        const OTPartyAccount* pAcct = it.second;
        assert_false(
            nullptr == pAcct,
            "Unexpected nullptr partyaccount pointer in party map.");

        if (lInput == pAcct->GetClosingTransNo()) { return true; }
    }

    return false;
}

void OTParty::GetAllTransactionNumbers(NumList& numlistOutput) const
{
    if (opening_trans_no_ > 0) { numlistOutput.Add(opening_trans_no_); }

    for (const auto& it : party_accounts_) {
        const OTPartyAccount* pAcct = it.second;
        assert_false(
            nullptr == pAcct,
            "Unexpected nullptr partyaccount pointer in party map.");

        const std::int64_t lTemp = pAcct->GetClosingTransNo();
        if (lTemp > 0) { numlistOutput.Add(lTemp); }
    }
}

// Only counts accounts authorized for str_agent_name.
//
auto OTParty::GetAccountCount(UnallocatedCString str_agent_name) const
    -> std::int32_t
{
    std::int32_t nCount = 0;

    for (const auto& it : party_accounts_) {
        const OTPartyAccount* pAcct = it.second;
        assert_false(
            nullptr == pAcct,
            "Unexpected nullptr partyaccount pointer in party map.");

        const String& strAgentName = pAcct->GetAgentName();

        if (strAgentName.Compare(str_agent_name.c_str())) { nCount++; }
    }

    return nCount;
}

auto OTParty::AddAgent(OTAgent& theAgent) -> bool
{
    const UnallocatedCString str_agent_name = theAgent.GetName().Get();

    if (!OTScriptable::ValidateName(str_agent_name)) {
        LogError()()("Failed validating Agent name.").Flush();
        return false;
    }

    auto it = agents_.find(str_agent_name);

    if (agents_.end() == it)  // If it wasn't already there...
    {
        // TODO: Validate here that the same agent isn't already on this party
        // under a different name.
        // Server either has to validate this as well, or be smart enough to
        // juggle the Nyms inside the
        // agents so that they aren't loaded twice.

        // Then insert it...
        agents_.insert(
            std::pair<UnallocatedCString, OTAgent*>(str_agent_name, &theAgent));

        // Make sure it has a pointer back to me.
        theAgent.SetParty(*this);

        return true;
    } else {
        LogConsole()()("Failed -- Agent was already there named ")(
            str_agent_name)(".")
            .Flush();
    }

    return false;
}

auto OTParty::AddAccount(
    const String& strAgentName,
    const String& strName,
    const String& strAcctID,
    const String& strInstrumentDefinitionID,
    std::int64_t lClosingTransNo) -> bool
{
    auto* pPartyAccount = new OTPartyAccount(
        api_,
        data_folder_,
        strName,
        strAgentName,
        strAcctID,
        strInstrumentDefinitionID,
        lClosingTransNo);
    assert_false(nullptr == pPartyAccount);

    if (!AddAccount(*pPartyAccount)) {
        delete pPartyAccount;
        return false;
    }

    return true;
}

auto OTParty::AddAccount(
    const String& strAgentName,
    const char* szAcctName,
    Account& theAccount,
    std::int64_t lClosingTransNo) -> bool
{
    auto* pPartyAccount = new OTPartyAccount(
        api_,
        data_folder_,
        szAcctName,
        strAgentName,
        theAccount,
        lClosingTransNo);
    assert_false(nullptr == pPartyAccount);

    if (!AddAccount(*pPartyAccount)) {
        delete pPartyAccount;
        return false;
    }

    return true;
}

auto OTParty::RemoveAccount(const UnallocatedCString str_Name) -> bool
{
    for (auto it = party_accounts_.begin(); it != party_accounts_.end(); ++it) {
        OTPartyAccount* pAcct = it->second;
        assert_false(nullptr == pAcct);

        const UnallocatedCString str_acct_name = pAcct->GetName().Get();

        if (0 == str_acct_name.compare(str_Name)) {
            party_accounts_.erase(it);
            delete pAcct;
            pAcct = nullptr;
            return true;
        }
    }

    return false;
}

auto OTParty::AddAccount(OTPartyAccount& thePartyAcct) -> bool
{
    const UnallocatedCString str_acct_name = thePartyAcct.GetName().Get();

    if (!OTScriptable::ValidateName(str_acct_name)) {
        LogError()()("Failed validating Account name.").Flush();
        return false;
    }

    auto it = party_accounts_.find(str_acct_name);

    if (party_accounts_.end() == it)  // If it wasn't already there...
    {
        // Todo:  Validate here that there isn't another account already on the
        // same party
        // that, while it has a different "account name" actually has the same
        // Account ID.
        // We do not want the same Account ID on multiple accounts. (Unless the
        // script
        // interpreter is going to be smart enough to make them available that
        // way without
        // ever loading the same account twice.) We can't otherwise take the
        // risk, si server
        //  will have to validate this unless it juggles the accounts like that.
        //

        // Then insert it...
        party_accounts_.insert(std::pair<UnallocatedCString, OTPartyAccount*>(
            str_acct_name, &thePartyAcct));

        // Make sure it has a pointer back to me.
        thePartyAcct.SetParty(*this);

        return true;
    } else {
        LogConsole()()("Failed -- Account was already on party "
                       "named ")(str_acct_name)(".")
            .Flush();
    }

    return false;
}

auto OTParty::GetClosingTransNo(UnallocatedCString str_for_acct_name) const
    -> std::int64_t
{
    auto it = party_accounts_.find(str_for_acct_name);

    if (party_accounts_.end() == it)  // If it wasn't already there...
    {
        LogConsole()()("Failed -- Account wasn't found: ")(
            str_for_acct_name)(".")
            .Flush();
        return 0;
    }

    OTPartyAccount* pPartyAccount = it->second;
    assert_false(nullptr == pPartyAccount);

    return pPartyAccount->GetClosingTransNo();
}

void OTParty::CleanupAgents()
{

    while (!agents_.empty()) {
        OTAgent* pTemp = agents_.begin()->second;
        assert_false(nullptr == pTemp);
        delete pTemp;
        pTemp = nullptr;
        agents_.erase(agents_.begin());
    }
}

void OTParty::CleanupAccounts()
{

    while (!party_accounts_.empty()) {
        OTPartyAccount* pTemp = party_accounts_.begin()->second;
        assert_false(nullptr == pTemp);
        delete pTemp;
        pTemp = nullptr;
        party_accounts_.erase(party_accounts_.begin());
    }
}

void OTParty::ClearTemporaryPointers()
{
    for (auto& it : agents_) {
        OTAgent* pAgent = it.second;
        assert_false(
            nullptr == pAgent,
            "Unexpected nullptr agent pointer in party map.");
    }
}

// as used "IN THE SCRIPT."
//
auto OTParty::GetPartyName(bool* pBoolSuccess) const -> UnallocatedCString
{
    UnallocatedCString retVal("");

    // "sales_director", "marketer", etc
    if (nullptr == party_name_) {
        if (nullptr != pBoolSuccess) { *pBoolSuccess = false; }

        return retVal;
    }

    if (nullptr != pBoolSuccess) { *pBoolSuccess = true; }

    retVal = *party_name_;

    return retVal;
}

auto OTParty::SetPartyName(const UnallocatedCString& str_party_name_input)
    -> bool
{
    if (!OTScriptable::ValidateName(str_party_name_input)) {
        LogError()()("Failed: Invalid name was passed in.").Flush();
        return false;
    }

    if (nullptr == party_name_) {
        assert_false(nullptr == (party_name_ = new UnallocatedCString));
    }

    *party_name_ = str_party_name_input;

    return true;
}

// ACTUAL PARTY OWNER (Only ONE of these can be true...)
// Debating whether these two functions should be private. (Should it matter to
// outsider?)
//
auto OTParty::IsNym() const -> bool
{
    // If the party is a Nym. (The party is the actual owner/beneficiary.)
    return party_is_nym_;
}

auto OTParty::IsEntity() const -> bool
{
    // If the party is an Entity. (Either way, the AGENT carries out all
    // wishes.)
    return !party_is_nym_;
}

// ACTUAL PARTY OWNER
//
auto OTParty::GetNymID(bool* pBoolSuccess) const -> UnallocatedCString
{
    if (IsNym() && (owner_id_.size() > 0)) {
        if (nullptr != pBoolSuccess) { *pBoolSuccess = true; }

        return owner_id_;
    }

    if (nullptr != pBoolSuccess) { *pBoolSuccess = false; }

    UnallocatedCString retVal("");

    return retVal;  // empty ID on failure.
}

auto OTParty::GetEntityID(bool* pBoolSuccess) const -> UnallocatedCString
{
    if (IsEntity() && (owner_id_.size() > 0)) {
        if (nullptr != pBoolSuccess) { *pBoolSuccess = true; }

        return owner_id_;
    }

    if (nullptr != pBoolSuccess) { *pBoolSuccess = false; }

    UnallocatedCString retVal("");

    return retVal;
}

auto OTParty::GetPartyID(bool* pBoolSuccess) const -> UnallocatedCString
{
    // If party is a Nym, this is the NymID. Else return EntityID().
    // if error, return false.

    if (IsNym()) { return GetNymID(pBoolSuccess); }

    return GetEntityID(pBoolSuccess);
}

// Some agents are passive (voting groups) and cannot behave actively, and so
// cannot do
// certain things that only Nyms can do. But they can still act as an agent in
// CERTAIN
// respects, so they are still allowed to do so. However, likely many functions
// will
// require that HasActiveAgent() be true for a party to do various actions.
// Attempts to
// do those actions otherwise will fail.
// It's almost a separate kind of party but not worthy of a separate class.
//
auto OTParty::HasActiveAgent() const -> bool
{
    // Loop through all agents and call IsAnIndividual() on each.
    // then return true if any is true. else return false;
    //
    for (const auto& it : agents_) {
        OTAgent* pAgent = it.second;
        assert_false(nullptr == pAgent);

        if (pAgent->IsAnIndividual()) { return true; }
    }

    return false;
}

/// Get Agent pointer by Name. Returns nullptr on failure.
///
auto OTParty::GetAgent(const UnallocatedCString& str_agent_name) const
    -> OTAgent*
{
    if (OTScriptable::ValidateName(str_agent_name)) {
        auto it = agents_.find(str_agent_name);

        if (agents_.end() != it)  // If we found something...
        {
            OTAgent* pAgent = it->second;
            assert_false(nullptr == pAgent);

            return pAgent;
        }
    } else {
        LogError()()("Failed: str_agent_name is invalid...").Flush();
    }

    return nullptr;
}

/// Get Agent pointer by Index. Returns nullptr on failure.
///
auto OTParty::GetAgentByIndex(std::int32_t nIndex) const -> OTAgent*
{
    if (!((nIndex >= 0) &&
          (nIndex < static_cast<std::int64_t>(agents_.size())))) {
        LogError()()("Index out of bounds: ")(nIndex)(".").Flush();
    } else {
        std::int32_t nLoopIndex = -1;

        for (const auto& it : agents_) {
            OTAgent* pAgent = it.second;
            assert_false(nullptr == pAgent);

            ++nLoopIndex;  // 0 on first iteration.

            if (nLoopIndex == nIndex) { return pAgent; }
        }
    }
    return nullptr;
}

// Get PartyAccount pointer by Name. Returns nullptr on failure.
//
auto OTParty::GetAccount(const UnallocatedCString& str_acct_name) const
    -> OTPartyAccount*
{
    //    otErr << "DEBUGGING OTParty::GetAccount: above find. str_acct_name: %s
    // \n", str_acct_name.c_str());

    if (OTScriptable::ValidateName(str_acct_name)) {
        auto it = party_accounts_.find(str_acct_name);

        if (party_accounts_.end() != it)  // If we found something...
        {
            OTPartyAccount* pAcct = it->second;
            assert_false(nullptr == pAcct);

            return pAcct;
        }
    } else {
        LogError()()("Failed: str_acct_name is invalid.").Flush();
    }

    return nullptr;
}

/// Get OTPartyAccount pointer by Index. Returns nullptr on failure.
///
auto OTParty::GetAccountByIndex(std::int32_t nIndex) -> OTPartyAccount*
{
    if (!((nIndex >= 0) &&
          (nIndex < static_cast<std::int64_t>(party_accounts_.size())))) {
        LogError()()("Index out of bounds: ")(nIndex)(".").Flush();
    } else {
        std::int32_t nLoopIndex = -1;

        for (auto& it : party_accounts_) {
            OTPartyAccount* pAcct = it.second;
            assert_false(nullptr == pAcct);

            ++nLoopIndex;  // 0 on first iteration.

            if (nLoopIndex == nIndex) { return pAcct; }
        }
    }
    return nullptr;
}

// Get PartyAccount pointer by Agent Name. (It just grabs the first one.)
//
// Returns nullptr on failure.
auto OTParty::GetAccountByAgent(const UnallocatedCString& str_agent_name)
    -> OTPartyAccount*
{
    if (OTScriptable::ValidateName(str_agent_name)) {
        for (auto& it : party_accounts_) {
            OTPartyAccount* pAcct = it.second;
            assert_false(nullptr == pAcct);

            if (pAcct->GetAgentName().Compare(str_agent_name.c_str())) {
                return pAcct;
            }
        }
    } else {
        LogError()()("Failed: str_agent_name is invalid.").Flush();
    }

    return nullptr;
}

// Get PartyAccount pointer by Acct ID.
//
// Returns nullptr on failure.
auto OTParty::GetAccountByID(const identifier::Account& theAcctID) const
    -> OTPartyAccount*
{
    for (const auto& it : party_accounts_) {
        OTPartyAccount* pAcct = it.second;
        assert_false(nullptr == pAcct);

        if (pAcct->IsAccountByID(theAcctID)) { return pAcct; }
    }

    return nullptr;
}

// bool OTPartyAccount::IsAccountByID(const identifier::Account& theAcctID)
// const

// If account is present for Party, return true.
auto OTParty::HasAccountByID(
    const identifier::Account& theAcctID,
    OTPartyAccount** ppPartyAccount) const -> bool
{
    for (const auto& it : party_accounts_) {
        OTPartyAccount* pAcct = it.second;
        assert_false(nullptr == pAcct);

        if (pAcct->IsAccountByID(theAcctID)) {
            if (nullptr != ppPartyAccount) { *ppPartyAccount = pAcct; }

            return true;
        }
    }

    return false;
}

// If account is present for Party, set account's pointer to theAccount and
// return true.
auto OTParty::HasAccount(
    const Account& theAccount,
    OTPartyAccount** ppPartyAccount) const -> bool
{
    for (const auto& it : party_accounts_) {
        OTPartyAccount* pAcct = it.second;
        assert_false(nullptr == pAcct);

        if (pAcct->IsAccount(theAccount)) {
            if (nullptr != ppPartyAccount) { *ppPartyAccount = pAcct; }

            return true;
        }
    }

    return false;
}

// Find out if theNym is an agent for Party.
// If so, make sure that agent has a pointer to theNym and return true.
// else return false.
//
auto OTParty::HasAgent(const identity::Nym& theNym, OTAgent** ppAgent) const
    -> bool
{
    for (const auto& it : agents_) {
        OTAgent* pAgent = it.second;
        assert_false(nullptr == pAgent);

        if (pAgent->IsValidSigner(theNym)) {
            if (nullptr != ppAgent) { *ppAgent = pAgent; }

            return true;
        }
    }

    return false;
}

auto OTParty::HasAgentByNymID(
    const identifier::Generic& theNymID,
    OTAgent** ppAgent) const -> bool
{
    for (const auto& it : agents_) {
        OTAgent* pAgent = it.second;
        assert_false(nullptr == pAgent);

        if (pAgent->IsValidSignerID(theNymID)) {
            if (nullptr != ppAgent) { *ppAgent = pAgent; }

            return true;
        }
    }

    return false;
}

// Find out if theNym is authorizing agent for Party. (Supplied opening
// transaction #) If so, make sure that agent has a pointer to theNym and return
// true. else return false.
auto OTParty::HasAuthorizingAgent(
    const identity::Nym& theNym,
    OTAgent** ppAgent) const -> bool  // ppAgent lets you get the agent ptr if
                                      // it was there.
{
    if (OTScriptable::ValidateName(authorizing_agent_)) {
        auto it = agents_.find(authorizing_agent_);

        if (agents_.end() != it)  // If we found something...
        {
            OTAgent* pAgent = it->second;
            assert_false(nullptr == pAgent);

            if (pAgent->IsValidSigner(theNym)) {
                // Optionally can pass in a pointer-to-pointer-to-Agent, in
                // order to get the Agent pointer back.
                if (nullptr != ppAgent) { *ppAgent = pAgent; }

                return true;
            }
        } else {  // found nothing.
            LogError()()("Named agent wasn't found "
                         "on list.")
                .Flush();
        }
    }

    return false;
}

auto OTParty::HasAuthorizingAgentByNymID(
    const identifier::Generic& theNymID,
    OTAgent** ppAgent) const -> bool  // ppAgent
                                      // lets you
                                      // get the
                                      // agent ptr
                                      // if it was
                                      // there.
{
    if (OTScriptable::ValidateName(authorizing_agent_)) {
        auto it = agents_.find(authorizing_agent_);

        if (agents_.end() != it)  // If we found something...
        {
            OTAgent* pAgent = it->second;
            assert_false(nullptr == pAgent);

            if (pAgent->IsValidSignerID(theNymID))  // if theNym is valid signer
                                                    // for pAgent.
            {
                // Optionally can pass in a pointer-to-pointer-to-Agent, in
                // order to get the Agent pointer back.
                if (nullptr != ppAgent) { *ppAgent = pAgent; }

                return true;
            }
        } else {  // found nothing.
            LogError()()("Named agent wasn't "
                         "found on list.")
                .Flush();
        }
    }

    return false;
}

// Load up the Nym that authorized the agreement for this party
// (the nym who supplied the opening trans# to sign it.)
//
// This function ASSUMES that you ALREADY called HasAuthorizingAgentNym(), for
// example
// to verify that the serverNym isn't the one you were looking for.
// This is a low-level function.

// ppAgent lets you get the agent ptr if it was there.
auto OTParty::LoadAuthorizingAgentNym(
    const identity::Nym& theSignerNym,
    OTAgent** ppAgent) -> Nym_p
{
    if (OTScriptable::ValidateName(authorizing_agent_)) {
        auto it = agents_.find(authorizing_agent_);

        if (agents_.end() != it)  // If we found something...
        {
            OTAgent* pAgent = it->second;
            assert_false(nullptr == pAgent);

            Nym_p pNym = nullptr;

            if (!pAgent->IsAnIndividual()) {
                LogError()()("This agent is not "
                             "an individual--there's no Nym to load.")
                    .Flush();
            } else if (nullptr == (pNym = pAgent->LoadNym())) {
                LogError()()("Failed loading "
                             "Nym.")
                    .Flush();
            } else {
                if (nullptr != ppAgent) {  // Pass the agent back, too, if it
                                           // was requested.
                    *ppAgent = pAgent;
                }

                return pNym;  // Success
            }
        } else {  // found nothing.
            LogError()()("Named agent wasn't "
                         "found on list.")
                .Flush();
        }
    }

    return nullptr;
}

auto OTParty::VerifyOwnershipOfAccount(const Account& theAccount) const -> bool
{
    if (IsNym())  // For those cases where the party is actually just a
                  // solitary Nym (not an entity.)
    {
        bool bNymID = false;
        const UnallocatedCString str_nym_id =
            GetNymID(&bNymID);  // If the party is a Nym, this is the Nym's
                                // ID. Otherwise this is false.

        if (!bNymID || (str_nym_id.size() <= 0)) {
            LogError()()("Although party is a "
                         "Nym, unable to retrieve NymID!")
                .Flush();
            return false;
        }

        const auto thePartyNymID = api_.Factory().NymIDFromBase58(str_nym_id);

        return theAccount.VerifyOwnerByID(thePartyNymID);
    } else if (IsEntity()) {
        LogError()()(
            "Error: Entities have not been implemented yet, but somehow this "
            "party is an entity.")
            .Flush();
    } else {
        LogError()()("Error: Unknown party type.").Flush();
    }

    return false;
}

// This is only for SmartContracts, NOT all scriptables.
//
auto OTParty::DropFinalReceiptToInboxes(
    const String& strNotaryID,
    const std::int64_t& lNewTransactionNumber,
    const String& strOrigCronItem,
    const PasswordPrompt& reason,
    OTString pstrNote,
    OTString pstrAttachment) -> bool
{
    bool bSuccess = true;  // Success is defined as "all inboxes were notified"

    OTSmartContract* pSmartContract = nullptr;

    if (nullptr == owner_agreement_) {
        LogError()()("Missing pointer to owner agreement.").Flush();
        return false;
    } else if (
        nullptr ==
        (pSmartContract = dynamic_cast<OTSmartContract*>(owner_agreement_))) {
        LogError()()("Can only drop finalReceipts for smart contracts.")
            .Flush();
        return false;
    }

    // By this point, we know pSmartContract is a good pointer.

    for (auto& it : party_accounts_) {
        OTPartyAccount* pAcct = it.second;
        assert_false(
            nullptr == pAcct,
            "Unexpected nullptr partyaccount pointer in party map.");

        if (false == pAcct->DropFinalReceiptToInbox(
                         strNotaryID,
                         *pSmartContract,
                         lNewTransactionNumber,
                         strOrigCronItem,
                         reason,
                         pstrNote,
                         pstrAttachment)) {
            LogError()()("Failed dropping final Receipt to agent's Inbox.")
                .Flush();
            bSuccess = false;  // Notice: no break. We still try to notify them
                               // all, even if one fails.
        }
    }

    return bSuccess;
}

// This is only for SmartContracts, NOT all scriptables.
//
auto OTParty::DropFinalReceiptToNymboxes(
    const std::int64_t& lNewTransactionNumber,
    const String& strOrigCronItem,
    const PasswordPrompt& reason,
    OTString pstrNote,
    OTString pstrAttachment) -> bool
{
    bool bSuccess =
        false;  // Success is defined as "at least one agent was notified"

    OTSmartContract* pSmartContract = nullptr;

    if (nullptr == owner_agreement_) {
        LogError()()("Missing pointer to "
                     "owner agreement.")
            .Flush();
        return false;
    } else if (
        nullptr ==
        (pSmartContract = dynamic_cast<OTSmartContract*>(owner_agreement_))) {
        LogError()()("Can only drop "
                     "finalReceipts for smart contracts.")
            .Flush();
        return false;
    }

    // By this point, we know pSmartContract is a good pointer.

    for (auto& it : agents_) {
        OTAgent* pAgent = it.second;
        assert_false(
            nullptr == pAgent,
            "Unexpected nullptr agent pointer in party map.");

        if (false == pAgent->DropFinalReceiptToNymbox(
                         *pSmartContract,
                         lNewTransactionNumber,
                         strOrigCronItem,
                         reason,
                         pstrNote,
                         pstrAttachment)) {
            LogError()()("Failed dropping "
                         "final Receipt to agent's Nymbox.")
                .Flush();
        } else {
            bSuccess = true;
        }
    }

    return bSuccess;
}

auto OTParty::SendNoticeToParty(
    const api::Session& api,
    bool bSuccessMsg,
    const identity::Nym& theServerNym,
    const identifier::Notary& theNotaryID,
    const std::int64_t& lNewTransactionNumber,
    const String& strReference,
    const PasswordPrompt& reason,
    OTString pstrNote,
    OTString pstrAttachment,
    identity::Nym* pActualNym) -> bool
{
    bool bSuccess =
        false;  // Success is defined as "at least one agent was notified"

    if (nullptr == owner_agreement_) {
        LogError()()("Missing pointer to owner agreement.").Flush();
        return false;
    }

    const std::int64_t lOpeningTransNo = GetOpeningTransNo();

    if (lOpeningTransNo > 0) {
        for (auto& it : agents_) {
            OTAgent* pAgent = it.second;
            assert_false(
                nullptr == pAgent,
                "Unexpected nullptr agent pointer in party map.");

            if (false == pAgent->DropServerNoticeToNymbox(
                             api,
                             bSuccessMsg,
                             theServerNym,
                             theNotaryID,
                             lNewTransactionNumber,
                             lOpeningTransNo,  // lInReferenceTo
                             strReference,
                             reason,
                             pstrNote,
                             pstrAttachment,
                             pActualNym)) {
                LogError()()("Failed dropping server notice to agent's Nymbox.")
                    .Flush();
            } else {
                bSuccess = true;
            }
        }
    }
    return bSuccess;
}

auto OTParty::LoadAndVerifyAssetAccounts(
    const String& strNotaryID,
    mapOfAccounts& map_Accts_Already_Loaded,
    mapOfAccounts& map_NewlyLoaded) -> bool
{
    UnallocatedSet<UnallocatedCString> theAcctIDSet;  // Make sure all the acct
                                                      // IDs are unique.

    for (auto& it_acct : party_accounts_) {
        const UnallocatedCString str_acct_name = it_acct.first;
        OTPartyAccount* pPartyAcct = it_acct.second;
        assert_true(pPartyAcct != nullptr);

        bool bHadToLoadtheAcctMyself = true;
        SharedAccount account;
        const String& strAcctID = pPartyAcct->GetAcctID();

        if (!strAcctID.Exists()) {
            LogConsole()()("Bad: Acct ID is "
                           "blank for account: ")(
                str_acct_name)(", on party: ")(GetPartyName())(".")
                .Flush();
            return false;
        }

        // Disallow duplicate Acct IDs.
        // (Only can use an acct once inside a smart contract.)
        //
        auto it_acct_id = theAcctIDSet.find(strAcctID.Get());

        if (theAcctIDSet.end() == it_acct_id)  // It's not already there (good).
        {
            theAcctIDSet.insert(strAcctID.Get());  // Save a copy so we can make
                                                   // sure there's no duplicate
                                                   // acct IDs. (Not allowed.)
        } else {
            LogConsole()()("Failure: Found a "
                           "duplicate Acct ID (")(strAcctID)("), on acct: ")(
                str_acct_name)(".")
                .Flush();
            return false;
        }

        // If it's there, it's mapped by Acct ID, so we can look it up.
        auto it = map_Accts_Already_Loaded.find(strAcctID.Get());

        if (map_Accts_Already_Loaded.end() != it) {
            account = it->second;

            assert_true(account);

            // Now we KNOW the Account is "already loaded" and we KNOW the
            // partyaccount has a POINTER to that Acct:
            //
            const bool bIsPartyAcct = pPartyAcct->IsAccount(account.get());
            assert_true(
                bIsPartyAcct,
                "Failed call:  pPartyAcct->IsAccount(*account);");  // assert
                                                                    // because
                                                                    // the Acct
                                                                    // was
                                                                    // already
                                                                    // mapped by
                                                                    // ID, so it
                                                                    // should
                                                                    // already
                                                                    // have been
                                                                    // validated.
            if (!bIsPartyAcct) {
                LogError()()("Failed call: "
                             "pPartyAcct->IsAccount(*account).")
                    .Flush();
            }

            bHadToLoadtheAcctMyself = false;  // Whew. The Acct was already
                                              // loaded. Found it. (And the ptr
                                              // is now set.)
        }

        // Looks like the Acct wasn't already loaded....
        // Let's load it up...
        //
        if (bHadToLoadtheAcctMyself == true) {
            if ((account = pPartyAcct->LoadAccount())) {
                LogConsole()()("Failed loading "
                               "Account with name: ")(
                    str_acct_name)(" and ID: ")(strAcctID)(".")
                    .Flush();
                return false;
            }
            // Successfully loaded the Acct! We add to this map so it gets
            // cleaned-up properly later.
            map_NewlyLoaded.emplace(strAcctID.Get(), std::move(account));
        }
    }

    return true;
}

// This is only meant to be used in OTSmartContract::VerifySmartContract() RIGHT
// AFTER the call
// to VerifyPartyAuthorization(). It ASSUMES the nyms and asset accounts are all
// loaded up, with
// internal pointers to them available.
//
auto OTParty::VerifyAccountsWithTheirAgents(
    const String& strNotaryID,
    const PasswordPrompt& reason,
    bool bBurnTransNo) -> bool
{
    assert_false(nullptr == owner_agreement_);

    bool bAllSuccessful = true;

    // By this time this function is called, ALL the Nyms and Asset Accounts
    // should ALREADY
    // be loaded up in memory!
    //
    for (auto& it : party_accounts_) {
        const UnallocatedCString str_acct_name = it.first;
        OTPartyAccount* pAcct = it.second;
        assert_false(
            nullptr == pAcct,
            "Unexpected nullptr partyaccount pointer in party map.");

        const bool bVerified = owner_agreement_->VerifyPartyAcctAuthorization(
            reason,
            *pAcct,  // The party is assumed to have been verified already via
                     // VerifyPartyAuthorization()
            strNotaryID,    // For verifying issued num, need the notaryID the #
                            // goes with.
            bBurnTransNo);  // bBurnTransNo=false ) // In
                            // Server::VerifySmartContract(), it not only
                            // wants to verify the closing # is properly issued,
                            // but it additionally wants to see that it hasn't
                            // been USED yet -- AND it wants to burn it, so it
                            // can't be used again!  This bool allows you to
                            // tell the function whether or not to do that.
        if (!bVerified)     // This mechanism is here because we still want
                            // to let them ALL verify, before returning
                            // false.
        {
            bAllSuccessful = false;  // That is, we don't want to return at the
                                     // first failure, but let them all go
                                     // through. (This is in order to keep the
                                     // output consistent.)
            LogConsole()()(
                "Ownership, agency, or potentially "
                "closing transaction # failed to verify on account: ")(
                str_acct_name)(".")
                .Flush();
        }
    }

    return bAllSuccessful;
}

// Done
// The party will use its authorizing agent.
//
auto OTParty::SignContract(Contract& theInput, const PasswordPrompt& reason)
    const -> bool
{
    if (GetAuthorizingAgentName().size() <= 0) {
        LogError()()("Error: Authorizing agent name is blank.").Flush();
        return false;
    }

    OTAgent* pAgent = GetAgent(GetAuthorizingAgentName());

    if (nullptr == pAgent) {
        LogError()()("Error: Unable to find Authorizing agent (")(
            GetAuthorizingAgentName())(") for party: ")(GetPartyName())(".")
            .Flush();
        return false;
    }

    return pAgent->SignContract(theInput, reason);
}

// for whichever partyaccounts have agents that happen to be loaded, this will
// harvest the closing trans#s.
// Calls OTAgent::HarvestTransactionNumber
void OTParty::HarvestClosingNumbers(
    const String& strNotaryID,
    const PasswordPrompt& reason)
{
    for (auto& it : party_accounts_) {
        OTPartyAccount* pAcct = it.second;

        assert_false(
            nullptr == pAcct,
            "Unexpected nullptr partyaccount pointer in party map.");

        if (pAcct->GetClosingTransNo() <= 0) { continue; }

        const UnallocatedCString str_agent_name(pAcct->GetAgentName().Get());

        if (str_agent_name.size() <= 0) {
            LogError()()("Missing agent name on party account: ")(
                pAcct->GetName())(".")
                .Flush();

            continue;
        }

        OTAgent* pAgent = GetAgent(str_agent_name);

        if (nullptr == pAgent) {
            LogError()()("Couldn't find agent (")(
                str_agent_name)(") for asset account: ")(pAcct->GetName())(".")
                .Flush();
        } else {
            pAgent->RecoverTransactionNumber(
                pAcct->GetClosingTransNo(), strNotaryID, reason);
        }
    }
}

// Done
// Calls OTAgent::HarvestTransactionNumber
//
void OTParty::recover_closing_numbers(
    OTAgent& theAgent,
    otx::context::Server& context) const
{
    for (const auto& it : party_accounts_) {
        OTPartyAccount* pAcct = it.second;

        assert_false(
            nullptr == pAcct,
            "Unexpected nullptr partyaccount pointer in partyaccount map.");

        if (pAcct->GetClosingTransNo() <= 0) { continue; }

        const UnallocatedCString str_agent_name(pAcct->GetAgentName().Get());

        if (str_agent_name.size() <= 0) { continue; }

        if (theAgent.GetName().Compare(str_agent_name.c_str())) {
            theAgent.RecoverTransactionNumber(
                pAcct->GetClosingTransNo(), context);
        }
        // We don't break here, on success, because this agent might represent
        // multiple accounts.
        // else nothing...
    }
}

// Done.
// IF theNym is one of my agents, then grab his numbers back for him.
// If he is NOT one of my agents, then do nothing.
void OTParty::HarvestClosingNumbers(otx::context::Server& context)
{
    OTAgent* pAgent = nullptr;

    if (HasAgent(*context.Signer(), &pAgent)) {

        assert_false(nullptr == pAgent);

        recover_closing_numbers(*pAgent, context);
    }
    // else nothing...
}

// Done
// IF theNym is one of my agents, then grab his opening number back for him.
// If he is NOT one of my agents, then do nothing.
void OTParty::HarvestOpeningNumber(otx::context::Server& context)
{
    OTAgent* pAgent = nullptr;

    if (HasAuthorizingAgent(*context.Signer(), &pAgent)) {
        assert_false(nullptr == pAgent);
        recover_opening_number(*pAgent, context);
    }
    // else no error, since many nyms could get passed in here (in a loop)
}  // The function above me, calls the one below.

void OTParty::recover_opening_number(
    OTAgent& theAgent,
    otx::context::Server& context) const
{
    if (!(GetAuthorizingAgentName().compare(theAgent.GetName().Get()) == 0)) {
        LogError()()("Error: Agent name doesn't match: ")(
            GetAuthorizingAgentName())(" / ")(theAgent.GetName())(".")
            .Flush();
    } else if (GetOpeningTransNo() > 0) {
        theAgent.RecoverTransactionNumber(GetOpeningTransNo(), context);
    } else {
        LogConsole()()("Nothing to harvest, it was already 0 for party: ")(
            GetPartyName())(".")
            .Flush();
    }
}

void OTParty::HarvestAllTransactionNumbers(otx::context::Server& context)
{
    HarvestOpeningNumber(context);
    HarvestClosingNumbers(context);
}

// Calls OTAgent::RemoveIssuedNumber (above)
void OTParty::CloseoutOpeningNumber(
    const String& strNotaryID,
    const PasswordPrompt& reason)
{
    if (GetAuthorizingAgentName().size() <= 0) {
        LogError()()("Error: Authorizing agent name is blank.").Flush();
        return;
    }

    OTAgent* pAgent = GetAgent(GetAuthorizingAgentName());

    if (nullptr == pAgent) {
        LogError()()("Error: Unable to find Authorizing agent (")(
            GetAuthorizingAgentName())(") for party: ")(GetPartyName())(".")
            .Flush();
    } else if (GetOpeningTransNo() > 0) {
        pAgent->RemoveIssuedNumber(GetOpeningTransNo(), strNotaryID, reason);
    } else {
        LogConsole()()("Nothing to closeout, it was already 0 for party: ")(
            GetPartyName())(".")
            .Flush();
    }
}

// Done
// This function ASSUMES that the internal Nym pointer (on the authorizing
// agent) is set,
// and also that the Nym pointer is set on the authorized agent for each asset
// account as well.
//
// The party is getting ready to CONFIRM the smartcontract, so he will have to
// provide
// the appropriate transaction #s to do so.  This is the function where he tries
// to reserve
// those. Client-side.
//
auto OTParty::ReserveTransNumsForConfirm(otx::context::Server& context) -> bool
{
    // RESERVE THE OPENING TRANSACTION NUMBER, LOCATED ON THE AUTHORIZING AGENT
    // FOR THIS PARTY.

    if (GetAuthorizingAgentName().size() <= 0) {
        LogConsole()()(
            "Failure: Authorizing "
            "agent's name is empty on this party: ")(GetPartyName())(".")
            .Flush();
        return false;
    }

    OTAgent* pMainAgent = GetAgent(GetAuthorizingAgentName());

    if (nullptr == pMainAgent) {
        LogConsole()()("Failure: Authorizing "
                       "agent (")(GetPartyName())(
            ") not found on this party: ")(GetAuthorizingAgentName())(".")
            .Flush();
        return false;
    }

    if (!pMainAgent->ReserveOpeningTransNum(context)) {
        LogConsole()()("Failure: Authorizing "
                       "agent (")(GetAuthorizingAgentName())(
            ") didn't have an opening transaction #, on party: ")(
            GetPartyName())(".")
            .Flush();
        return false;
    }
    // BELOW THIS POINT, the OPENING trans# has been RESERVED and
    // must be RETRIEVED in the event of failure, using this call:
    // HarvestAllTransactionNumbers(context);

    // RESERVE THE CLOSING TRANSACTION NUMBER for each asset account, LOCATED ON
    // ITS AUTHORIZED AGENT.
    // (Do this for each account on this party.)
    //
    for (auto& it : party_accounts_) {
        OTPartyAccount* pPartyAccount = it.second;
        assert_false(nullptr == pPartyAccount);

        if (!pPartyAccount->GetAgentName().Exists()) {
            LogConsole()()("Failure: Authorized "
                           "agent name is blank for account: ")(
                pPartyAccount->GetName())(".")
                .Flush();
            // We have to put them back before returning, since this function
            // has failed.
            HarvestAllTransactionNumbers(context);

            return false;
        }

        OTAgent* pAgent = GetAgent(pPartyAccount->GetAgentName().Get());

        if (nullptr == pAgent) {
            LogConsole()()("Failure: Unable to "
                           "locate Authorized agent for account: ")(
                pPartyAccount->GetName())(".")
                .Flush();
            // We have to put them back before returning, since this function
            // has failed.
            HarvestAllTransactionNumbers(context);

            return false;
        }
        // Below this point, pAgent is good.

        if (!pAgent->ReserveClosingTransNum(context, *pPartyAccount)) {
            LogConsole()()("Failure: "
                           "Authorizing agent (")(GetAuthorizingAgentName())(
                ") didn't have a closing transaction #, on party: ")(
                GetPartyName())(".")
                .Flush();
            // We have to put them back before returning, since this function
            // has failed.
            HarvestAllTransactionNumbers(context);

            return false;
        }
        // BELOW THIS POINT, the CLOSING TRANSACTION # has been reserved for
        // this account, and MUST BE RETRIEVED in the event of failure.
    }

    // BY THIS POINT, we have successfully reserved the Opening Transaction #
    // for the party (from its
    // authorizing agent) and we have also successfully reserved Closing
    // Transaction #s for EACH ASSET
    // ACCOUNT, from the authorized agent for each asset account.
    // Therefore we have reserved ALL the needed transaction #s, so let's return
    // true.

    return true;
}

void OTParty::Serialize(
    Tag& parent,
    bool bCalculatingID,
    bool bSpecifyInstrumentDefinitionID,
    bool bSpecifyParties) const
{
    TagPtr pTag(new Tag("party"));

    const auto numAgents = agents_.size();
    const auto numAccounts = party_accounts_.size();

    pTag->add_attribute("name", GetPartyName());
    pTag->add_attribute(
        "ownerType", bCalculatingID ? "" : (party_is_nym_ ? "nym" : "entity"));
    pTag->add_attribute(
        "ownerID", (bCalculatingID && !bSpecifyParties) ? "" : owner_id_);
    pTag->add_attribute(
        "openingTransNo",
        std::to_string(bCalculatingID ? 0 : opening_trans_no_));
    pTag->add_attribute(
        "signedCopyProvided",
        formatBool((!bCalculatingID && my_signed_copy_->Exists())));
    // When an agent activates this contract, it's HIS opening trans#.
    pTag->add_attribute(
        "authorizingAgent", bCalculatingID ? "" : authorizing_agent_);
    pTag->add_attribute(
        "numAgents", std::to_string(bCalculatingID ? 0 : numAgents));
    pTag->add_attribute("numAccounts", std::to_string(numAccounts));

    if (!bCalculatingID) {
        for (const auto& it : agents_) {
            OTAgent* pAgent = it.second;
            assert_false(nullptr == pAgent);
            pAgent->Serialize(*pTag);
        }
    }

    for (const auto& it : party_accounts_) {
        OTPartyAccount* pAcct = it.second;
        assert_false(nullptr == pAcct);
        pAcct->Serialize(*pTag, bCalculatingID, bSpecifyInstrumentDefinitionID);
    }

    if (!bCalculatingID && my_signed_copy_->Exists()) {
        auto ascTemp = Armored::Factory(api_.Crypto(), my_signed_copy_);
        pTag->add_tag("mySignedCopy", ascTemp->Get());
    }

    parent.add_tag(pTag);
}

// Register the variables of a specific Bylaw into the Script interpreter,
// so we can execute a script.
//
void OTParty::RegisterAccountsForExecution(OTScript& theScript)
{
    for (auto& it : party_accounts_) {
        const UnallocatedCString str_acct_name = it.first;
        OTPartyAccount* pAccount = it.second;
        assert_true((nullptr != pAccount) && (str_acct_name.size() > 0));

        pAccount->RegisterForExecution(theScript);
    }
}

// Done.
auto OTParty::Compare(const OTParty& rhs) const -> bool
{
    const UnallocatedCString str_party_name(rhs.GetPartyName());

    if (!(str_party_name.compare(GetPartyName()) == 0)) {
        LogConsole()()("Names don't match. ")(GetPartyName())(" / ")(
            str_party_name)(".")
            .Flush();
        return false;
    }

    // The party might first be added WITHOUT filling out the Nym/Agent info.
    // As long as the party's name is right, and the accounts are all there with
    // the
    // correct instrument definition IDs, then it should matter if LATER, when
    // the party
    // CONFIRMS
    // the agreement, he supplies himself as an entity or a Nym, or whether he
    // supplies this
    // agent or that agent.  That information is important and is stored, but is
    // not relevant
    // for a Compare().

    if ((GetOpeningTransNo() > 0) && (rhs.GetOpeningTransNo() > 0) &&
        (GetOpeningTransNo() != rhs.GetOpeningTransNo())) {
        LogConsole()()("Opening transaction numbers don't match "
                       "for party ")(GetPartyName())(". (")(
            GetOpeningTransNo())(" / ")(rhs.GetOpeningTransNo())(").")
            .Flush();
        return false;
    }

    if ((GetPartyID().size() > 0) && (rhs.GetPartyID().size() > 0) &&
        !(GetPartyID().compare(rhs.GetPartyID()) == 0)) {
        LogConsole()()("Party IDs don't match for party ")(GetPartyName())(
            ". (")(GetPartyID())(" / ")(rhs.GetPartyID())(").")
            .Flush();
        return false;
    }

    if ((GetAuthorizingAgentName().size() > 0) &&
        (rhs.GetAuthorizingAgentName().size() > 0) &&
        !(GetAuthorizingAgentName().compare(rhs.GetAuthorizingAgentName()) ==
          0)) {
        LogConsole()()(
            "Authorizing agent names don't match for "
            "party ")(GetPartyName())(". (")(GetAuthorizingAgentName())(" / ")(
            rhs.GetAuthorizingAgentName())(").")
            .Flush();
        return false;
    }

    // No need to compare agents... right?
    //
    //    mapOfAgents            agents_; // These are owned.

    if (GetAccountCount() != rhs.GetAccountCount()) {
        LogConsole()()("Mismatched number of accounts when "
                       "comparing party ")(GetPartyName())(".")
            .Flush();
        return false;
    }

    for (const auto& it : party_accounts_) {
        const UnallocatedCString str_acct_name = it.first;
        OTPartyAccount* pAcct = it.second;
        assert_false(nullptr == pAcct);

        OTPartyAccount* p2 = rhs.GetAccount(str_acct_name);

        if (nullptr == p2) {
            LogConsole()()("Unable to find Account ")(
                str_acct_name)(" on rhs, when comparing party ")(
                GetPartyName())(".")
                .Flush();
            return false;
        }
        if (!pAcct->Compare(*p2)) {
            LogConsole()()("Accounts (")(
                str_acct_name)(") don't match when comparing party ")(
                GetPartyName())(".")
                .Flush();
            return false;
        }
    }

    return true;
}

// When confirming a party, a new version replaces the original. This is part of
// that process.
// *this is the old one, and theParty is the new one.
//
auto OTParty::CopyAcctsToConfirmingParty(OTParty& theParty) const -> bool
{
    theParty.CleanupAccounts();  // (We're going to copy our own accounts into
                                 // theParty.)

    for (const auto& it : party_accounts_) {
        const UnallocatedCString str_acct_name = it.first;
        OTPartyAccount* pAcct = it.second;
        assert_false(nullptr == pAcct);

        if (false == theParty.AddAccount(
                         pAcct->GetAgentName(),
                         pAcct->GetName(),
                         pAcct->GetAcctID(),
                         pAcct->GetInstrumentDefinitionID(),
                         pAcct->GetClosingTransNo())) {
            LogConsole()()("Unable to add Account ")(
                str_acct_name)(", when copying from *this party ")(
                GetPartyName())(".")
                .Flush();
            return false;
        }
    }

    return true;
}

OTParty::~OTParty()
{
    CleanupAgents();
    CleanupAccounts();

    if (nullptr != party_name_) { delete party_name_; }
    party_name_ = nullptr;

    owner_agreement_ = nullptr;
}
}  // namespace opentxs
