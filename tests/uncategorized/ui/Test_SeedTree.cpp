// Copyright (c) 2010-2023 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include <opentxs/opentxs.hpp>
#include <atomic>
#include <optional>

#include "ottest/data/crypto/PaymentCodeV3.hpp"
#include "ottest/fixtures/common/Counter.hpp"
#include "ottest/fixtures/common/User.hpp"
#include "ottest/fixtures/ui/SeedTree.hpp"

namespace ottest
{
TEST_F(SeedTree, initialize_opentxs) { init_seed_tree(api_, counter_); }

TEST_F(SeedTree, empty)
{
    const auto expected = SeedTreeData{};

    ASSERT_TRUE(wait_for_counter(counter_));
    EXPECT_TRUE(check_seed_tree(api_, expected));
    EXPECT_TRUE(check_seed_tree_qt(api_, expected));
}

TEST_F(SeedTree, create_nyms)
{
    counter_.expected_ += 7;

    {
        const auto& v = GetPaymentCodeVector3().alice_;
        alex_.emplace(v.words_, alex_name_);
        alex_->init(api_);
    }
    {
        const auto& v = GetPaymentCodeVector3().alice_;
        bob_.emplace(v.words_, bob_name_);
        bob_->init(api_, ot::identity::Type::individual, 1);
    }
    {
        chris_.emplace(pkt_words_, chris_name_, pkt_passphrase_);
        chris_->init(
            api_,
            ot::identity::Type::individual,
            0,
            ot::crypto::SeedStyle::PKT);
    }

    const auto expected = SeedTreeData{{
        {seed_1_id_,
         "Unnamed seed: BIP-39 (default)",
         ot::crypto::SeedStyle::BIP39,
         {
             {0,
              alex_->nym_id_.asBase58(alex_->api_->Crypto()),
              ot::UnallocatedCString{alex_name_} + " (default)"},
             {1, bob_->nym_id_.asBase58(alex_->api_->Crypto()), bob_name_},
         }},
        {seed_2_id_,
         "Unnamed seed: Legacy pktwallet",
         ot::crypto::SeedStyle::PKT,
         {
             {0, chris_->nym_id_.asBase58(alex_->api_->Crypto()), chris_name_},
         }},
    }};

    ASSERT_TRUE(wait_for_counter(counter_));
    EXPECT_TRUE(check_seed_tree(api_, expected));
    EXPECT_TRUE(check_seed_tree_qt(api_, expected));
}

TEST_F(SeedTree, add_seed)
{
    counter_.expected_ += 1;
    const auto& v = GetPaymentCodeVector3().bob_;
    api_.Crypto().Seed().ImportSeed(
        api_.Factory().SecretFromText(v.words_),
        api_.Factory().Secret(0),
        ot::crypto::SeedStyle::BIP39,
        ot::crypto::Language::en,
        reason_,
        "Imported");

    const auto expected = SeedTreeData{{
        {seed_1_id_,
         "Unnamed seed: BIP-39 (default)",
         ot::crypto::SeedStyle::BIP39,
         {
             {0,
              alex_->nym_id_.asBase58(alex_->api_->Crypto()),
              ot::UnallocatedCString{alex_name_} + " (default)"},
             {1, bob_->nym_id_.asBase58(alex_->api_->Crypto()), bob_name_},
         }},
        {seed_3_id_, "Imported: BIP-39", ot::crypto::SeedStyle::BIP39, {}},
        {seed_2_id_,
         "Unnamed seed: Legacy pktwallet",
         ot::crypto::SeedStyle::PKT,
         {
             {0, chris_->nym_id_.asBase58(alex_->api_->Crypto()), chris_name_},
         }},
    }};

    ASSERT_TRUE(wait_for_counter(counter_));
    EXPECT_TRUE(check_seed_tree(api_, expected));
    EXPECT_TRUE(check_seed_tree_qt(api_, expected));
}

TEST_F(SeedTree, rename_nym)
{
    counter_.expected_ += 1;
    {
        auto editor = api_.Wallet().mutable_Nym(alex_->nym_id_, reason_);
        editor.SetScope(editor.Type(), daniel_name_, true, reason_);
    }

    const auto expected = SeedTreeData{{
        {seed_1_id_,
         "Unnamed seed: BIP-39 (default)",
         ot::crypto::SeedStyle::BIP39,
         {
             {0,
              alex_->nym_id_.asBase58(alex_->api_->Crypto()),
              ot::UnallocatedCString{daniel_name_} + " (default)"},
             {1, bob_->nym_id_.asBase58(alex_->api_->Crypto()), bob_name_},
         }},
        {seed_3_id_, "Imported: BIP-39", ot::crypto::SeedStyle::BIP39, {}},
        {seed_2_id_,
         "Unnamed seed: Legacy pktwallet",
         ot::crypto::SeedStyle::PKT,
         {
             {0, chris_->nym_id_.asBase58(alex_->api_->Crypto()), chris_name_},
         }},
    }};

    ASSERT_TRUE(wait_for_counter(counter_));
    EXPECT_TRUE(check_seed_tree(api_, expected));
    EXPECT_TRUE(check_seed_tree_qt(api_, expected));
}

TEST_F(SeedTree, rename_seed)
{
    counter_.expected_ += 1;
    api_.Crypto().Seed().SetSeedComment(seed_2_id_, "Backup");
    const auto expected = SeedTreeData{{
        {seed_1_id_,
         "Unnamed seed: BIP-39 (default)",
         ot::crypto::SeedStyle::BIP39,
         {
             {0,
              alex_->nym_id_.asBase58(alex_->api_->Crypto()),
              ot::UnallocatedCString{daniel_name_} + " (default)"},
             {1, bob_->nym_id_.asBase58(alex_->api_->Crypto()), bob_name_},
         }},
        {seed_2_id_,
         "Backup: Legacy pktwallet",
         ot::crypto::SeedStyle::PKT,
         {
             {0, chris_->nym_id_.asBase58(alex_->api_->Crypto()), chris_name_},
         }},
        {seed_3_id_, "Imported: BIP-39", ot::crypto::SeedStyle::BIP39, {}},
    }};

    ASSERT_TRUE(wait_for_counter(counter_));
    EXPECT_TRUE(check_seed_tree(api_, expected));
    EXPECT_TRUE(check_seed_tree_qt(api_, expected));
}

TEST_F(SeedTree, change_default_seed)
{
    counter_.expected_ += 3;
    api_.Crypto().Seed().SetDefault(seed_2_id_);
    const auto expected = SeedTreeData{{
        {seed_2_id_,
         "Backup: Legacy pktwallet (default)",
         ot::crypto::SeedStyle::PKT,
         {
             {0, chris_->nym_id_.asBase58(alex_->api_->Crypto()), chris_name_},
         }},
        {seed_3_id_, "Imported: BIP-39", ot::crypto::SeedStyle::BIP39, {}},
        {seed_1_id_,
         "Unnamed seed: BIP-39",
         ot::crypto::SeedStyle::BIP39,
         {
             {0,
              alex_->nym_id_.asBase58(alex_->api_->Crypto()),
              ot::UnallocatedCString{daniel_name_} + " (default)"},
             {1, bob_->nym_id_.asBase58(alex_->api_->Crypto()), bob_name_},
         }},
    }};

    ASSERT_TRUE(wait_for_counter(counter_));
    EXPECT_TRUE(check_seed_tree(api_, expected));
    EXPECT_TRUE(check_seed_tree_qt(api_, expected));
}

TEST_F(SeedTree, change_default_nym)
{
    counter_.expected_ += 3;
    api_.Wallet().SetDefaultNym(bob_->nym_id_);
    const auto expected = SeedTreeData{{
        {seed_2_id_,
         "Backup: Legacy pktwallet (default)",
         ot::crypto::SeedStyle::PKT,
         {
             {0, chris_->nym_id_.asBase58(alex_->api_->Crypto()), chris_name_},
         }},
        {seed_3_id_, "Imported: BIP-39", ot::crypto::SeedStyle::BIP39, {}},
        {seed_1_id_,
         "Unnamed seed: BIP-39",
         ot::crypto::SeedStyle::BIP39,
         {
             {0, alex_->nym_id_.asBase58(alex_->api_->Crypto()), daniel_name_},
             {1,
              bob_->nym_id_.asBase58(alex_->api_->Crypto()),
              ot::UnallocatedCString{bob_name_} + " (default)"},
         }},
    }};

    ASSERT_TRUE(wait_for_counter(counter_));
    EXPECT_TRUE(check_seed_tree(api_, expected));
    EXPECT_TRUE(check_seed_tree_qt(api_, expected));
}
}  // namespace ottest
