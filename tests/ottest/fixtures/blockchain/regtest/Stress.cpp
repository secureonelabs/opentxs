// Copyright (c) 2010-2023 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ottest/fixtures/blockchain/regtest/Stress.hpp"  // IWYU pragma: associated

#include <opentxs/opentxs.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string_view>

#include "ottest/data/crypto/PaymentCodeV3.hpp"
#include "ottest/fixtures/blockchain/Common.hpp"
#include "ottest/fixtures/blockchain/ScanListener.hpp"
#include "ottest/fixtures/blockchain/regtest/Normal.hpp"

namespace ottest
{
using namespace opentxs::literals;
using namespace std::literals;

bool first_block_{true};

ot::Nym_p Regtest_stress::alex_p_;
ot::Nym_p Regtest_stress::bob_p_;
Regtest_stress::Transactions Regtest_stress::transactions_;
std::unique_ptr<ScanListener> Regtest_stress::listener_alex_p_;
std::unique_ptr<ScanListener> Regtest_stress::listener_bob_p_;

auto Regtest_stress::GetAddresses() noexcept
    -> ot::UnallocatedVector<ot::UnallocatedCString>
{
    auto output = ot::UnallocatedVector<ot::UnallocatedCString>{};
    output.reserve(tx_per_block_);
    const auto reason = client_2_.Factory().PasswordPrompt(__func__);
    using enum opentxs::blockchain::crypto::SubaccountType;
    auto hd = client_2_.Crypto()
                  .Blockchain()
                  .Account(bob_.ID(), test_chain_)
                  .GetSubaccounts(HD);
    auto& bob = hd[0].asDeterministic().asHD();
    const auto indices = bob.Reserve(Subchain::External, tx_per_block_, reason);

    opentxs::assert_true(indices.size() == tx_per_block_);

    for (const auto index : indices) {
        const auto& element = bob.BalanceElement(Subchain::External, index);
        using Style = ot::blockchain::crypto::AddressStyle;
        const auto& address =
            output.emplace_back(element.Address(Style::p2pkh));

        opentxs::assert_true(false == address.empty());
    }

    return output;
}

auto Regtest_stress::Shutdown() noexcept -> void
{
    listener_bob_p_.reset();
    listener_alex_p_.reset();
    transactions_.clear();
    bob_p_.reset();
    alex_p_.reset();
    Regtest_fixture_normal::Shutdown();
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-reference"  // NOLINT
Regtest_stress::Regtest_stress()
    : Regtest_fixture_normal(ot_, 2)
    , alex_([&]() -> const ot::identity::Nym& {
        if (!alex_p_) {
            const auto reason = client_1_.Factory().PasswordPrompt(__func__);
            const auto& vector = GetPaymentCodeVector3().alice_;
            const auto seedID = [&] {
                const auto words =
                    client_1_.Factory().SecretFromText(vector.words_);
                const auto phrase = client_1_.Factory().Secret(0);

                return client_1_.Crypto().Seed().ImportSeed(
                    words,
                    phrase,
                    ot::crypto::SeedStyle::BIP39,
                    ot::crypto::Language::en,
                    reason);
            }();

            alex_p_ = client_1_.Wallet().Nym(
                {client_1_.Factory(), seedID, 0}, reason, "Alex");

            opentxs::assert_false(nullptr == alex_p_);
            opentxs::assert_true(
                alex_p_->PaymentCodePublic().asBase58() ==
                vector.payment_code_);

            client_1_.Crypto().Blockchain().NewHDSubaccount(
                alex_p_->ID(),
                ot::blockchain::crypto::HDProtocol::BIP_44,
                test_chain_,
                reason);
        }

        opentxs::assert_false(nullptr == alex_p_);

        return *alex_p_;
    }())
    , bob_([&]() -> const ot::identity::Nym& {
        if (!bob_p_) {
            const auto reason = client_2_.Factory().PasswordPrompt(__func__);
            const auto& vector = GetPaymentCodeVector3().bob_;
            const auto seedID = [&] {
                const auto words =
                    client_2_.Factory().SecretFromText(vector.words_);
                const auto phrase = client_2_.Factory().Secret(0);

                return client_2_.Crypto().Seed().ImportSeed(
                    words,
                    phrase,
                    ot::crypto::SeedStyle::BIP39,
                    ot::crypto::Language::en,
                    reason);
            }();

            bob_p_ = client_2_.Wallet().Nym(
                {client_2_.Factory(), seedID, 0}, reason, "Alex");

            opentxs::assert_false(nullptr == bob_p_);

            client_2_.Crypto().Blockchain().NewHDSubaccount(
                bob_p_->ID(),
                ot::blockchain::crypto::HDProtocol::BIP_44,
                test_chain_,
                reason);
        }

        opentxs::assert_false(nullptr == bob_p_);

        return *bob_p_;
    }())
    , alex_account_(client_1_.Crypto()
                        .Blockchain()
                        .Account(alex_.ID(), test_chain_)
                        .GetSubaccounts(
                            opentxs::blockchain::crypto::SubaccountType::HD)[0]
                        .asDeterministic()
                        .asHD())
    , bob_account_(client_2_.Crypto()
                       .Blockchain()
                       .Account(bob_.ID(), test_chain_)
                       .GetSubaccounts(
                           opentxs::blockchain::crypto::SubaccountType::HD)[0]
                       .asDeterministic()
                       .asHD())
    , expected_account_alex_(alex_account_.Parent().AccountID())
    , expected_account_bob_(bob_account_.Parent().AccountID())
    , expected_notary_(client_1_.UI().BlockchainNotaryID(test_chain_))
    , expected_unit_(client_1_.UI().BlockchainUnitID(test_chain_))
    , expected_display_unit_(u8"UNITTEST"sv)
    , expected_account_name_(u8"On chain UNITTEST (this device)"sv)
    , expected_notary_name_(u8"Unit Test Simulation"sv)
    , memo_outgoing_("memo for outgoing transaction")
    , expected_account_type_(ot::AccountType::Blockchain)
    , expected_unit_type_(ot::UnitType::Regtest)
    , mine_to_alex_([&](Height height) -> Transaction {
        using OutputBuilder = ot::blockchain::OutputBuilder;
        auto builder = [&] {
            auto output = ot::UnallocatedVector<OutputBuilder>{};
            const auto reason = client_1_.Factory().PasswordPrompt(__func__);
            const auto keys = ot::UnallocatedSet<ot::blockchain::crypto::Key>{};
            const auto target = [] {
                if (first_block_) {
                    first_block_ = false;

                    return tx_per_block_ * 2u;
                } else {

                    return tx_per_block_;
                }
            }();
            const auto indices =
                alex_account_.Reserve(Subchain::External, target, reason);

            opentxs::assert_true(indices.size() == target);

            for (const auto index : indices) {
                const auto& element =
                    alex_account_.BalanceElement(Subchain::External, index);
                output.emplace_back(
                    amount_,
                    miner_.Factory().BitcoinScriptP2PK(
                        test_chain_, element.Key(), {}),
                    keys);
            }

            return output;
        }();
        auto output = miner_.Factory().BlockchainTransaction(
            test_chain_, height, builder, coinbase_fun_, 2, {});
        transactions_.emplace_back(output.ID());

        return output;
    })
    , listener_alex_([&]() -> ScanListener& {
        if (!listener_alex_p_) {
            listener_alex_p_ = std::make_unique<ScanListener>(client_1_);
        }

        opentxs::assert_false(nullptr == listener_alex_p_);

        return *listener_alex_p_;
    }())
    , listener_bob_([&]() -> ScanListener& {
        if (!listener_bob_p_) {
            listener_bob_p_ = std::make_unique<ScanListener>(client_2_);
        }

        opentxs::assert_false(nullptr == listener_bob_p_);

        return *listener_bob_p_;
    }())
{
}
#pragma GCC diagnostic pop
}  // namespace ottest
