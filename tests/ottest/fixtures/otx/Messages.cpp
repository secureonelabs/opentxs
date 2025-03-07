// Copyright (c) 2010-2023 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/fixtures/otx/Messages.hpp"  // IWYU pragma: associated

#include <opentxs/opentxs.hpp>
#include <memory>
#include <string_view>

#include "internal/core/contract/ServerContract.hpp"
#include "opentxs/api/session/Wallet.internal.hpp"
#include "ottest/env/OTTestEnvironment.hpp"

namespace ottest
{
namespace ot = opentxs;
using namespace std::literals;

bool init_{false};

const ot::crypto::SeedID Messages::SeedA_{};
const ot::UnallocatedCString Messages::Alice_{""};
const ot::identifier::Nym Messages::alice_nym_id_{};

Messages::Messages()
    : client_(dynamic_cast<const ot::api::session::Client&>(
          OTTestEnvironment::GetOT().StartClientSession(0)))
    , server_(dynamic_cast<const ot::api::session::Notary&>(
          OTTestEnvironment::GetOT().StartNotarySession(0)))
    , reason_c_(client_.Factory().PasswordPrompt(__func__))
    , reason_s_(server_.Factory().PasswordPrompt(__func__))
    , server_id_(server_.ID())
    , server_contract_(server_.Wallet().Internal().Server(server_id_))
{
    if (false == init_) { init(); }
}

void Messages::import_server_contract(
    const ot::contract::Server& contract,
    const ot::api::session::Client& client)
{
    auto bytes = ot::Space{};
    server_contract_->Serialize(ot::writer(bytes), true);
    auto clientVersion = client.Wallet().Internal().Server(ot::reader(bytes));
    client.OTX().SetIntroductionServer(clientVersion);
}

void Messages::init()
{
    const_cast<ot::crypto::SeedID&>(SeedA_) = client_.Crypto().Seed().ImportSeed(
        client_.Factory().SecretFromText(
            "spike nominee miss inquiry fee nothing belt list other daughter leave valley twelve gossip paper"sv),
        client_.Factory().SecretFromText(""sv),
        opentxs::crypto::SeedStyle::BIP39,
        opentxs::crypto::Language::en,
        client_.Factory().PasswordPrompt("Importing a BIP-39 seed"));
    const_cast<ot::identifier::Nym&>(alice_nym_id_) =
        client_.Wallet()
            .Nym({client_.Factory(), SeedA_, 0}, reason_c_, "Alice")
            ->ID();
    const_cast<ot::UnallocatedCString&>(Alice_) =
        alice_nym_id_.asBase58(client_.Crypto());

    opentxs::assert_true(false == server_id_.empty());

    import_server_contract(server_contract_, client_);

    init_ = true;
}

}  // namespace ottest
