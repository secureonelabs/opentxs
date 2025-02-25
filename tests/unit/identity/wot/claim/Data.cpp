// Copyright (c) 2010-2023 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include <opentxs/opentxs.hpp>
#include <memory>
#include <span>
#include <utility>

#include "internal/core/String.hpp"
#include "internal/util/Pimpl.hpp"
#include "ottest/fixtures/contact/ContactData.hpp"
#include "ottest/fixtures/contact/ContactItem.hpp"

namespace ottest
{
namespace ot = opentxs;
namespace claim = ot::identity::wot::claim;
using namespace std::literals;

auto add_contract(
    const claim::Data& data,
    const ot::UnallocatedCString& id,
    const ot::UnitType type,
    const bool active,
    const bool primary) -> claim::Data;
auto add_contract(
    const claim::Data& data,
    const ot::UnallocatedCString& id,
    const ot::UnitType type,
    const bool active,
    const bool primary) -> claim::Data
{
    return data.AddContract(id, type, active, primary);
}

auto add_email(
    const claim::Data& data,
    const ot::UnallocatedCString& id,
    const bool active,
    const bool primary) -> claim::Data;
auto add_email(
    const claim::Data& data,
    const ot::UnallocatedCString& id,
    const bool active,
    const bool primary) -> claim::Data
{
    return data.AddEmail(id, active, primary);
}

auto add_payment_code(
    const claim::Data& data,
    const ot::UnallocatedCString& id,
    const ot::UnitType type,
    const bool active,
    const bool primary) -> claim::Data;
auto add_payment_code(
    const claim::Data& data,
    const ot::UnallocatedCString& id,
    const ot::UnitType type,
    const bool active,
    const bool primary) -> claim::Data
{
    return data.AddPaymentCode(id, type, active, primary);
}

auto add_phone_number(
    const claim::Data& data,
    const ot::UnallocatedCString& id,
    const bool active,
    const bool primary) -> claim::Data;
auto add_phone_number(
    const claim::Data& data,
    const ot::UnallocatedCString& id,
    const bool active,
    const bool primary) -> claim::Data
{
    return data.AddPhoneNumber(id, active, primary);
}

static const ot::UnallocatedCString expectedStringOutput =
    ot::UnallocatedCString{"Version "} +
    std::to_string(opentxs::identity::wot::claim::DefaultVersion()) +
    ot::UnallocatedCString(
        " contact data\nSections found: 1\n- Section: Identifier, version: ") +
    std::to_string(opentxs::identity::wot::claim::DefaultVersion()) +
    ot::UnallocatedCString{
        " containing 1 item(s).\n-- Item type: \"employee of\", value: "
        "\"activeContactItemValue\", start: 0, end: 0, version: "} +
    std::to_string(opentxs::identity::wot::claim::DefaultVersion()) +
    ot::UnallocatedCString{"\n--- Attributes: Active \n"};

TEST_F(ContactData, first_constructor)
{
    const std::shared_ptr<claim::Section> section1(new claim::Section(
        client_1_,
        "testContactSectionNym1",
        opentxs::identity::wot::claim::DefaultVersion(),
        opentxs::identity::wot::claim::DefaultVersion(),
        claim::SectionType::Identifier,
        active_contact_item_));

    const claim::Data::SectionMap map{{section1->Type(), section1}};

    const claim::Data contactData(
        client_1_,
        nym_id_1_.asBase58(client_1_.Crypto()),
        opentxs::identity::wot::claim::DefaultVersion(),
        opentxs::identity::wot::claim::DefaultVersion(),
        map);
    ASSERT_EQ(
        opentxs::identity::wot::claim::DefaultVersion(), contactData.Version());
    ASSERT_NE(nullptr, contactData.Section(claim::SectionType::Identifier));
    ASSERT_NE(
        nullptr,
        contactData.Group(
            claim::SectionType::Identifier, claim::ClaimType::Employee));
    ASSERT_TRUE(contactData.HaveClaim(
        claim::SectionType::Identifier,
        claim::ClaimType::Employee,
        active_contact_item_->Value()));
}

TEST_F(ContactData, first_constructor_no_sections)
{
    const claim::Data contactData(
        client_1_,
        nym_id_1_.asBase58(client_1_.Crypto()),
        opentxs::identity::wot::claim::DefaultVersion(),
        opentxs::identity::wot::claim::DefaultVersion(),
        {});
    ASSERT_EQ(
        opentxs::identity::wot::claim::DefaultVersion(), contactData.Version());
}

TEST_F(ContactData, first_constructor_different_versions)
{
    const claim::Data contactData(
        client_1_,
        nym_id_1_.asBase58(client_1_.Crypto()),
        opentxs::identity::wot::claim::DefaultVersion() - 1,  // previous
                                                              // version
        opentxs::identity::wot::claim::DefaultVersion(),
        {});
    ASSERT_EQ(
        opentxs::identity::wot::claim::DefaultVersion(), contactData.Version());
}

TEST_F(ContactData, copy_constructor)
{
    const std::shared_ptr<claim::Section> section1(new claim::Section(
        client_1_,
        "testContactSectionNym1",
        opentxs::identity::wot::claim::DefaultVersion(),
        opentxs::identity::wot::claim::DefaultVersion(),
        claim::SectionType::Identifier,
        active_contact_item_));

    const claim::Data::SectionMap map{{section1->Type(), section1}};

    const claim::Data contactData(
        client_1_,
        nym_id_1_.asBase58(client_1_.Crypto()),
        opentxs::identity::wot::claim::DefaultVersion(),
        opentxs::identity::wot::claim::DefaultVersion(),
        map);

    const claim::Data copiedContactData(contactData);

    ASSERT_EQ(
        opentxs::identity::wot::claim::DefaultVersion(),
        copiedContactData.Version());
    ASSERT_NE(
        nullptr, copiedContactData.Section(claim::SectionType::Identifier));
    ASSERT_NE(
        nullptr,
        copiedContactData.Group(
            claim::SectionType::Identifier, claim::ClaimType::Employee));
    ASSERT_TRUE(copiedContactData.HaveClaim(
        claim::SectionType::Identifier,
        claim::ClaimType::Employee,
        active_contact_item_->Value()));
}

TEST_F(ContactData, operator_plus)
{
    const auto data1 = contact_data_.AddItem(active_contact_item_);
    // Add a ContactData object with a section of the same type.
    const auto contactItem2 = std::make_shared<claim::Item>(
        claim_to_contact_item(client_1_.Factory().Claim(
            nym_id_1_,
            claim::SectionType::Identifier,
            claim::ClaimType::Employee,
            "contactItemValue2",
            active_attr_)));
    const auto group2 = std::make_shared<claim::Group>(
        "contactGroup2", claim::SectionType::Identifier, contactItem2);
    const auto section2 = std::make_shared<claim::Section>(
        client_1_,
        "contactSectionNym2",
        opentxs::identity::wot::claim::DefaultVersion(),
        opentxs::identity::wot::claim::DefaultVersion(),
        claim::SectionType::Identifier,
        claim::Section::GroupMap{{contactItem2->Type(), group2}});
    const auto data2 = claim::Data{
        client_1_,
        nym_id_3_.asBase58(client_1_.Crypto()),
        opentxs::identity::wot::claim::DefaultVersion(),
        opentxs::identity::wot::claim::DefaultVersion(),
        claim::Data::SectionMap{{claim::SectionType::Identifier, section2}}};
    const auto data3 = data1 + data2;

    // Verify the section exists.
    ASSERT_NE(nullptr, data3.Section(claim::SectionType::Identifier));
    // Verify it has one group.
    ASSERT_EQ(1, data3.Section(claim::SectionType::Identifier)->Size());
    // Verify the group exists.
    ASSERT_NE(
        nullptr,
        data3.Group(
            claim::SectionType::Identifier, claim::ClaimType::Employee));
    // Verify it has two items.
    ASSERT_EQ(
        2,
        data3.Group(claim::SectionType::Identifier, claim::ClaimType::Employee)
            ->Size());

    // Add a ContactData object with a section of a different type.
    const auto contactItem4 = std::make_shared<claim::Item>(
        claim_to_contact_item(client_1_.Factory().Claim(
            nym_id_1_,
            claim::SectionType::Address,
            claim::ClaimType::Physical,
            "contactItemValue4",
            active_attr_)));
    const auto group4 = std::make_shared<claim::Group>(
        "contactGroup4", claim::SectionType::Address, contactItem4);
    const auto section4 = std::make_shared<claim::Section>(
        client_1_,
        "contactSectionNym4",
        opentxs::identity::wot::claim::DefaultVersion(),
        opentxs::identity::wot::claim::DefaultVersion(),
        claim::SectionType::Address,
        claim::Section::GroupMap{{contactItem4->Type(), group4}});
    const auto data4 = claim::Data{
        client_1_,
        nym_id_4_.asBase58(client_1_.Crypto()),
        opentxs::identity::wot::claim::DefaultVersion(),
        opentxs::identity::wot::claim::DefaultVersion(),
        claim::Data::SectionMap{{claim::SectionType::Address, section4}}};
    const auto data5 = data3 + data4;

    // Verify the first section exists.
    ASSERT_NE(nullptr, data5.Section(claim::SectionType::Identifier));
    // Verify it has one group.
    ASSERT_EQ(1, data5.Section(claim::SectionType::Identifier)->Size());
    // Verify the group exists.
    ASSERT_NE(
        nullptr,
        data5.Group(
            claim::SectionType::Identifier, claim::ClaimType::Employee));
    // Verify it has two items.
    ASSERT_EQ(
        2,
        data5.Group(claim::SectionType::Identifier, claim::ClaimType::Employee)
            ->Size());

    // Verify the second section exists.
    ASSERT_NE(nullptr, data5.Section(claim::SectionType::Address));
    // Verify it has one group.
    ASSERT_EQ(1, data5.Section(claim::SectionType::Address)->Size());
    // Verify the group exists.
    ASSERT_NE(
        nullptr,
        data5.Group(claim::SectionType::Address, claim::ClaimType::Physical));
    // Verify it has one item.
    ASSERT_EQ(
        1,
        data5.Group(claim::SectionType::Address, claim::ClaimType::Physical)
            ->Size());
}

TEST_F(ContactData, operator_plus_different_version)
{
    // rhs version less than lhs
    const claim::Data contactData2(
        client_1_,
        nym_id_1_.asBase58(client_1_.Crypto()),
        opentxs::identity::wot::claim::DefaultVersion() - 1,
        opentxs::identity::wot::claim::DefaultVersion() - 1,
        {});

    const auto contactData3 = contact_data_ + contactData2;
    // Verify the new contact data has the latest version.
    ASSERT_EQ(
        opentxs::identity::wot::claim::DefaultVersion(),
        contactData3.Version());

    // lhs version less than rhs
    const auto contactData4 = contactData2 + contact_data_;
    // Verify the new contact data has the latest version.
    ASSERT_EQ(
        opentxs::identity::wot::claim::DefaultVersion(),
        contactData4.Version());
}

TEST_F(ContactData, operator_string)
{
    const auto data1 = contact_data_.AddItem(active_contact_item_);
    const ot::UnallocatedCString dataString = data1;
    ASSERT_EQ(expectedStringOutput, dataString);
}

TEST_F(ContactData, Serialize)
{
    const auto data1 = contact_data_.AddItem(active_contact_item_);

    // Serialize without ids.
    auto bytes = ot::Space{};
    EXPECT_TRUE(data1.Serialize(ot::writer(bytes), false));

    auto restored1 = claim::Data{
        client_1_, "ContactDataNym1", data1.Version(), ot::reader(bytes)};

    ASSERT_EQ(restored1.Version(), data1.Version());
    auto section_iterator = restored1.begin();
    auto section_name = section_iterator->first;
    ASSERT_EQ(section_name, claim::SectionType::Identifier);
    auto section1 = section_iterator->second;
    ASSERT_TRUE(section1);
    auto group_iterator = section1->begin();
    ASSERT_EQ(group_iterator->first, claim::ClaimType::Employee);
    auto group1 = group_iterator->second;
    ASSERT_TRUE(group1);
    auto item_iterator = group1->begin();
    auto contact_item = item_iterator->second;
    ASSERT_TRUE(contact_item);
    ASSERT_EQ(active_contact_item_->Value(), contact_item->Value());
    ASSERT_EQ(active_contact_item_->Version(), contact_item->Version());
    ASSERT_EQ(active_contact_item_->Type(), contact_item->Type());
    ASSERT_EQ(active_contact_item_->Start(), contact_item->Start());
    ASSERT_EQ(active_contact_item_->End(), contact_item->End());

    // Serialize with ids.
    EXPECT_TRUE(data1.Serialize(ot::writer(bytes), true));

    auto restored2 = claim::Data{
        client_1_, "ContactDataNym1", data1.Version(), ot::reader(bytes)};

    ASSERT_EQ(restored2.Version(), data1.Version());
    section_iterator = restored2.begin();
    section_name = section_iterator->first;
    ASSERT_EQ(section_name, claim::SectionType::Identifier);
    section1 = section_iterator->second;
    ASSERT_TRUE(section1);
    group_iterator = section1->begin();
    ASSERT_EQ(group_iterator->first, claim::ClaimType::Employee);
    group1 = group_iterator->second;
    ASSERT_TRUE(group1);
    item_iterator = group1->begin();
    contact_item = item_iterator->second;
    ASSERT_TRUE(contact_item);
    ASSERT_EQ(active_contact_item_->Value(), contact_item->Value());
    ASSERT_EQ(active_contact_item_->Version(), contact_item->Version());
    ASSERT_EQ(active_contact_item_->Type(), contact_item->Type());
    ASSERT_EQ(active_contact_item_->Start(), contact_item->Start());
    ASSERT_EQ(active_contact_item_->End(), contact_item->End());
}

TEST_F(ContactData, AddContract)
{
    testAddItemMethod(add_contract, claim::SectionType::Contract);
}

TEST_F(ContactData, AddContract_different_versions)
{
    testAddItemMethod(
        add_contract,
        claim::SectionType::Contract,
        3,  // version of CONTACTSECTION_CONTRACT section before CITEMTYPE_BCH
            // was added
        4);
}

TEST_F(ContactData, AddEmail)
{
    testAddItemMethod2(
        add_email, claim::SectionType::Communication, claim::ClaimType::Email);
}

// Nothing to test for required version of contact section for email
// because all current contact item types have been available for all
// versions of CONTACTSECTION_COMMUNICATION section.

// TEST_F(ContactData, AddEmail_different_versions)
//{
//    testAddItemMethod2(
//        std::mem_fn<claim::ContactData(
//            const ot::UnallocatedCString&, const bool, const bool) const>(
//            &claim::ContactData::AddEmail),
//        claim::ContactSectionName::Communication,
//        claim::ClaimType::Email,
//        5,   // Change this to the old version of the section when a new
//             // version is added with new item types.
//        5);  // Change this to the version of the section with the new
//             // item type.
//}

TEST_F(ContactData, AddItem_claim)
{
    static constexpr auto attrib = {claim::Attribute::Active};
    const auto claim = client_1_.Factory().Claim(
        nym_id_1_,
        claim::SectionType::Contract,
        claim::ClaimType::Usd,
        "contactItemValue",
        attrib);
    const auto data1 = contact_data_.AddItem(claim);
    // Verify the section was added.
    ASSERT_NE(nullptr, data1.Section(claim::SectionType::Contract));
    // Verify the group was added.
    ASSERT_NE(
        nullptr,
        data1.Group(claim::SectionType::Contract, claim::ClaimType::Usd));
    ASSERT_TRUE(data1.HaveClaim(
        claim::SectionType::Contract,
        claim::ClaimType::Usd,
        "contactItemValue"));
}

TEST_F(ContactData, AddItem_claim_different_versions)
{
    const auto group1 = std::make_shared<claim::Group>(
        "contactGroup1",
        claim::SectionType::Contract,
        claim::ClaimType::Bch,
        claim::Group::ItemMap{});

    const auto section1 = std::make_shared<claim::Section>(
        client_1_,
        "contactSectionNym1",
        3,  // version of CONTACTSECTION_CONTRACT section before
            // CITEMTYPE_BCH was added
        3,
        claim::SectionType::Contract,
        claim::Section::GroupMap{{claim::ClaimType::Bch, group1}});

    const claim::Data data1(
        client_1_,
        nym_id_2_.asBase58(client_1_.Crypto()),
        3,  // version of CONTACTSECTION_CONTRACT section before CITEMTYPE_BCH
            // was added
        3,
        claim::Data::SectionMap{{claim::SectionType::Contract, section1}});

    static constexpr auto attrib = {claim::Attribute::Active};
    const auto claim = client_1_.Factory().Claim(
        nym_id_2_,
        claim::SectionType::Contract,
        claim::ClaimType::Bch,
        "contactItemValue",
        attrib);

    const auto data2 = data1.AddItem(claim);

    ASSERT_EQ(4, data2.Version());
}

TEST_F(ContactData, AddItem_item)
{
    // Add an item to a ContactData with no section.
    const auto data1 = contact_data_.AddItem(active_contact_item_);
    // Verify the section was added.
    ASSERT_NE(nullptr, data1.Section(claim::SectionType::Identifier));
    // Verify the group was added.
    ASSERT_NE(
        nullptr,
        data1.Group(
            claim::SectionType::Identifier, claim::ClaimType::Employee));
    // Verify the item was added.
    ASSERT_TRUE(data1.HaveClaim(
        active_contact_item_->Section(),
        active_contact_item_->Type(),
        active_contact_item_->Value()));

    // Add an item to a ContactData with a section.
    const auto contactItem2 = std::make_shared<claim::Item>(
        claim_to_contact_item(client_1_.Factory().Claim(
            nym_id_1_,
            claim::SectionType::Identifier,
            claim::ClaimType::Employee,
            "contactItemValue2",
            active_attr_)));
    const auto data2 = data1.AddItem(contactItem2);
    // Verify the item was added.
    ASSERT_TRUE(data2.HaveClaim(
        contactItem2->Section(), contactItem2->Type(), contactItem2->Value()));
    // Verify the group has two items.
    ASSERT_EQ(
        2,
        data2.Group(claim::SectionType::Identifier, claim::ClaimType::Employee)
            ->Size());
}

TEST_F(ContactData, AddItem_item_different_versions)
{
    const auto group1 = std::make_shared<claim::Group>(
        "contactGroup1",
        claim::SectionType::Contract,
        claim::ClaimType::Bch,
        claim::Group::ItemMap{});

    const auto section1 = std::make_shared<claim::Section>(
        client_1_,
        "contactSectionNym1",
        3,  // version of CONTACTSECTION_CONTRACT section before
            // CITEMTYPE_BCH was added
        3,
        claim::SectionType::Contract,
        claim::Section::GroupMap{{claim::ClaimType::Bch, group1}});

    const claim::Data data1(
        client_1_,
        nym_id_2_.asBase58(client_1_.Crypto()),
        3,  // version of CONTACTSECTION_CONTRACT section before CITEMTYPE_BCH
            // was added
        3,
        claim::Data::SectionMap{{claim::SectionType::Contract, section1}});

    const auto contactItem1 = std::make_shared<claim::Item>(
        claim_to_contact_item(client_1_.Factory().Claim(
            nym_id_1_,
            claim::SectionType::Contract,
            claim::ClaimType::Bch,
            "contactItemValue1",
            active_attr_)));

    const auto data2 = data1.AddItem(contactItem1);

    ASSERT_EQ(4, data2.Version());
}

TEST_F(ContactData, AddPaymentCode)
{
    testAddItemMethod(add_payment_code, claim::SectionType::Procedure);
}

TEST_F(ContactData, AddPaymentCode_different_versions)
{
    testAddItemMethod(
        add_contract,
        claim::SectionType::Procedure,
        3,  // version of CONTACTSECTION_PROCEDURE section before CITEMTYPE_BCH
            // was added
        4);
}

TEST_F(ContactData, AddPhoneNumber)
{
    testAddItemMethod2(
        add_phone_number,
        claim::SectionType::Communication,
        claim::ClaimType::Phone);
}

// Nothing to test for required version of contact section for phone number
// because all current contact item types have been available for all
// versions of CONTACTSECTION_COMMUNICATION section.

// TEST_F(ContactData, AddPhoneNumber_different_versions)
//{
//    testAddItemMethod2(
//        std::mem_fn<claim::ContactData(
//            const ot::UnallocatedCString&, const bool, const bool) const>(
//            &claim::ContactData::AddPhoneNumber),
//        claim::ContactSectionName::Communication,
//        claim::ClaimType::Phone,
//        5,   // Change this to the old version of the section when a new
//             // version is added with new item types.
//        5);  // Change this to the version of the section with the new
//             // item type.
//}

TEST_F(ContactData, AddPreferredOTServer)
{
    // Add a server to a group with no primary.
    const auto group1 = std::make_shared<claim::Group>(
        "contactGroup1",
        claim::SectionType::Communication,
        claim::ClaimType::Opentxs,
        claim::Group::ItemMap{});

    const auto section1 = std::make_shared<claim::Section>(
        client_1_,
        "contactSectionNym1",
        opentxs::identity::wot::claim::DefaultVersion(),
        opentxs::identity::wot::claim::DefaultVersion(),
        claim::SectionType::Communication,
        claim::Section::GroupMap{{claim::ClaimType::Opentxs, group1}});

    const claim::Data data1(
        client_1_,
        nym_id_2_.asBase58(client_1_.Crypto()),
        opentxs::identity::wot::claim::DefaultVersion(),
        opentxs::identity::wot::claim::DefaultVersion(),
        claim::Data::SectionMap{{claim::SectionType::Communication, section1}});

    const ot::identifier::Generic serverIdentifier1 =
        ot::identity::credential::Contact::ClaimID(
            client_1_,
            nym_id_2_,
            claim::SectionType::Communication,
            claim::ClaimType::Opentxs,
            {},
            {},
            "serverID1"s,
            "",
            ot::identity::wot::claim::DefaultVersion());
    const auto data2 = data1.AddPreferredOTServer(serverIdentifier1, false);

    // Verify that the item was made primary.
    const auto identifier1 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_2_,
        claim::SectionType::Communication,
        claim::ClaimType::Opentxs,
        {},
        {},
        ot::String::Factory(serverIdentifier1, client_1_.Crypto())->Get(),
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto contactItem1 = data2.Claim(identifier1);
    ASSERT_NE(nullptr, contactItem1);
    ASSERT_TRUE(contactItem1->HasAttribute(
        opentxs::identity::wot::claim::Attribute::Primary));

    // Add a server to a group with a primary.
    const ot::identifier::Generic serverIdentifier2 =
        ot::identity::credential::Contact::ClaimID(
            client_1_,
            nym_id_2_,
            claim::SectionType::Communication,
            claim::ClaimType::Opentxs,
            {},
            {},
            "serverID2"s,
            "",
            ot::identity::wot::claim::DefaultVersion());
    const auto data3 = data2.AddPreferredOTServer(serverIdentifier2, false);

    // Verify that the item wasn't made primary.
    const auto identifier2 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_2_,
        claim::SectionType::Communication,
        claim::ClaimType::Opentxs,
        {},
        {},
        ot::String::Factory(serverIdentifier2, client_1_.Crypto())->Get(),
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto contactItem2 = data3.Claim(identifier2);
    ASSERT_NE(nullptr, contactItem2);
    ASSERT_FALSE(contactItem2->HasAttribute(
        opentxs::identity::wot::claim::Attribute::Primary));

    // Add a server to a ContactData with no group.
    const ot::identifier::Generic serverIdentifier3 =
        ot::identity::credential::Contact::ClaimID(
            client_1_,
            nym_id_1_,
            claim::SectionType::Communication,
            claim::ClaimType::Opentxs,
            {},
            {},
            "serverID3"s,
            "",
            ot::identity::wot::claim::DefaultVersion());
    const auto data4 =
        contact_data_.AddPreferredOTServer(serverIdentifier3, false);

    // Verify the group was created.
    ASSERT_NE(
        nullptr,
        data4.Group(
            claim::SectionType::Communication, claim::ClaimType::Opentxs));
    // Verify that the item was made primary.
    const auto identifier3 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_1_,
        claim::SectionType::Communication,
        claim::ClaimType::Opentxs,
        {},
        {},
        ot::String::Factory(serverIdentifier3, client_1_.Crypto())->Get(),
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto contactItem3 = data4.Claim(identifier3);
    ASSERT_NE(nullptr, contactItem3);
    ASSERT_TRUE(contactItem3->HasAttribute(
        opentxs::identity::wot::claim::Attribute::Primary));

    // Add a primary server.
    const ot::identifier::Generic serverIdentifier4 =
        ot::identity::credential::Contact::ClaimID(
            client_1_,
            nym_id_1_,
            claim::SectionType::Communication,
            claim::ClaimType::Opentxs,
            {},
            {},
            "serverID4"s,
            "",
            ot::identity::wot::claim::DefaultVersion());
    const auto data5 = data4.AddPreferredOTServer(serverIdentifier4, true);

    // Verify that the item was made primary.
    const auto identifier4 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_1_,
        claim::SectionType::Communication,
        claim::ClaimType::Opentxs,
        {},
        {},
        ot::String::Factory(serverIdentifier4, client_1_.Crypto())->Get(),
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto contactItem4 = data5.Claim(identifier4);
    ASSERT_NE(nullptr, contactItem4);
    ASSERT_TRUE(contactItem4->HasAttribute(
        opentxs::identity::wot::claim::Attribute::Primary));
    // Verify the previous preferred server is no longer primary.
    const auto contactItem5 = data5.Claim(identifier3);
    ASSERT_NE(nullptr, contactItem5);
    ASSERT_FALSE(contactItem5->HasAttribute(
        opentxs::identity::wot::claim::Attribute::Primary));
}

// Nothing to test for required version of contact section for OTServer
// because CMITEMTYPE_OPENTXS has been available for all versions of
// CONTACTSECTION_COMMUNICATION section.

// TEST_F(ContactData, AddPreferredOTServer_different_versions)
//{
//}

TEST_F(ContactData, AddSocialMediaProfile)
{
    // Add a profile that only resides in the profile section.

    // Add a profile to a contact with no primary profile.
    const auto data2 = contact_data_.AddSocialMediaProfile(
        "profileValue1", claim::ClaimType::Aboutme, false, false);
    // Verify that the item was made primary.
    const auto identifier1 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_1_,
        claim::SectionType::Profile,
        claim::ClaimType::Aboutme,
        {},
        {},
        "profileValue1",
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto contactItem1 = data2.Claim(identifier1);
    ASSERT_NE(nullptr, contactItem1);
    ASSERT_TRUE(contactItem1->HasAttribute(
        opentxs::identity::wot::claim::Attribute::Primary));

    // Add a primary profile.
    const auto data3 = data2.AddSocialMediaProfile(
        "profileValue2", claim::ClaimType::Aboutme, true, false);
    // Verify that the item was made primary.
    const auto identifier2 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_1_,
        claim::SectionType::Profile,
        claim::ClaimType::Aboutme,
        {},
        {},
        "profileValue2",
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto contactItem2 = data3.Claim(identifier2);
    ASSERT_NE(nullptr, contactItem2);
    ASSERT_TRUE(contactItem2->HasAttribute(
        opentxs::identity::wot::claim::Attribute::Primary));

    // Add an active profile.
    const auto data4 = data3.AddSocialMediaProfile(
        "profileValue3", claim::ClaimType::Aboutme, false, true);
    // Verify that the item was made active.
    const auto identifier3 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_1_,
        claim::SectionType::Profile,
        claim::ClaimType::Aboutme,
        {},
        {},
        "profileValue3",
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto contactItem3 = data4.Claim(identifier3);
    ASSERT_NE(nullptr, contactItem3);
    ASSERT_TRUE(contactItem3->HasAttribute(
        opentxs::identity::wot::claim::Attribute::Active));

    // Add a profile that resides in the profile and communication sections.

    const auto data5 = contact_data_.AddSocialMediaProfile(
        "profileValue4", claim::ClaimType::Linkedin, false, false);
    // Verify that it was added to the profile section.
    const auto identifier4 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_1_,
        claim::SectionType::Profile,
        claim::ClaimType::Linkedin,
        {},
        {},
        "profileValue4",
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto contactItem4 = data5.Claim(identifier4);
    ASSERT_NE(nullptr, contactItem4);
    // Verify that it was added to the communication section.
    const auto identifier5 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_1_,
        claim::SectionType::Communication,
        claim::ClaimType::Linkedin,
        {},
        {},
        "profileValue4",
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto contactItem5 = data5.Claim(identifier5);
    ASSERT_NE(nullptr, contactItem5);

    // Add a profile that resides in the profile and identifier sections.

    const auto data6 = data5.AddSocialMediaProfile(
        "profileValue5", claim::ClaimType::Yahoo, false, false);
    // Verify that it was added to the profile section.
    const auto identifier6 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_1_,
        claim::SectionType::Profile,
        claim::ClaimType::Yahoo,
        {},
        {},
        "profileValue5",
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto contactItem6 = data6.Claim(identifier6);
    ASSERT_NE(nullptr, contactItem6);
    // Verify that it was added to the identifier section.
    const auto identifier7 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_1_,
        claim::SectionType::Identifier,
        claim::ClaimType::Yahoo,
        {},
        {},
        "profileValue5",
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto contactItem7 = data6.Claim(identifier7);
    ASSERT_NE(nullptr, contactItem7);

    // Add a profile that resides in all three sections.

    const auto data7 = data6.AddSocialMediaProfile(
        "profileValue6", claim::ClaimType::Twitter, false, false);
    // Verify that it was added to the profile section.
    const auto identifier8 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_1_,
        claim::SectionType::Profile,
        claim::ClaimType::Twitter,
        {},
        {},
        "profileValue6",
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto contactItem8 = data7.Claim(identifier8);
    ASSERT_NE(nullptr, contactItem8);
    // Verify that it was added to the communication section.
    const auto identifier9 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_1_,
        claim::SectionType::Communication,
        claim::ClaimType::Twitter,
        {},
        {},
        "profileValue6",
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto contactItem9 = data7.Claim(identifier9);
    ASSERT_NE(nullptr, contactItem9);
    // Verify that it was added to the identifier section.
    const auto identifier10 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_1_,
        claim::SectionType::Identifier,
        claim::ClaimType::Twitter,
        {},
        {},
        "profileValue6",
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto contactItem10 = data7.Claim(identifier10);
    ASSERT_NE(nullptr, contactItem10);
}

// Nothing to test for required version of contact sections for social media
// profiles because all current contact item types have been available for
// all versions of CONTACTSECTION_COMMUNICATION, CONTACTSECTION_IDENTIFIER,
// and CONTACTSECTION_PROFILE sections.

// TEST_F(ContactData, AddSocialMediaProfile_different_versions)
//{
//    // Add a profile to the CONTACTSECTION_PROFILE section.
//    testAddItemMethod3(
//        std::mem_fn<claim::ContactData(
//            const ot::UnallocatedCString&,
//            const ot::protobuf::ContactSectionName,
//            const claim::,
//            const bool,
//            const bool)
//            const>(&claim::ContactData::AddSocialMediaProfile),
//        claim::ContactSectionName::Profile,
//        claim::ClaimType::Twitter,
//        5,   // Change this to the old version of the section when a new
//             // version is added with new item types.
//        5);  // Change this to the version of the section with the new
//             // item type.
//}

TEST_F(ContactData, BestEmail)
{
    // Add a non-active, non-primary email.
    const auto data1 = contact_data_.AddEmail("emailValue", false, false);
    // Verify it is the best email.
    ASSERT_STREQ("emailValue", data1.BestEmail().c_str());

    // Add an active, non-primary email.
    const auto data2 = contact_data_.AddEmail("activeEmailValue", false, true);
    // Verify it is the best email.
    ASSERT_STREQ("activeEmailValue", data2.BestEmail().c_str());

    // Add an active email to a contact data with a primary email (data1).
    const auto data3 = data1.AddEmail("activeEmailValue", false, true);
    // Verify the primary email is the best.
    ASSERT_STREQ("emailValue", data3.BestEmail().c_str());

    // Add a new primary email.
    const auto data4 = data3.AddEmail("primaryEmailValue", true, false);
    // Verify it is the best email.
    ASSERT_STREQ("primaryEmailValue", data4.BestEmail().c_str());
}

TEST_F(ContactData, BestPhoneNumber)
{
    // Add a non-active, non-primary phone number.
    const auto data1 =
        contact_data_.AddPhoneNumber("phoneNumberValue", false, false);
    // Verify it is the best phone number.
    ASSERT_STREQ("phoneNumberValue", data1.BestPhoneNumber().c_str());

    // Add an active, non-primary phone number.
    const auto data2 =
        contact_data_.AddPhoneNumber("activePhoneNumberValue", false, true);
    // Verify it is the best phone number.
    ASSERT_STREQ("activePhoneNumberValue", data2.BestPhoneNumber().c_str());

    // Add an active phone number to a contact data with a primary phone number
    // (data1).
    const auto data3 =
        data1.AddPhoneNumber("activePhoneNumberValue", false, true);
    // Verify the primary phone number is the best.
    ASSERT_STREQ("phoneNumberValue", data3.BestPhoneNumber().c_str());

    // Add a new primary phone number.
    const auto data4 =
        data3.AddPhoneNumber("primaryPhoneNumberValue", true, false);
    // Verify it is the best phone number.
    ASSERT_STREQ("primaryPhoneNumberValue", data4.BestPhoneNumber().c_str());
}

TEST_F(ContactData, BestSocialMediaProfile)
{
    // Add a non-active, non-primary profile.
    const auto data1 = contact_data_.AddSocialMediaProfile(
        "profileValue", claim::ClaimType::Facebook, false, false);
    // Verify it is the best profile.
    ASSERT_STREQ(
        "profileValue",
        data1.BestSocialMediaProfile(claim::ClaimType::Facebook).c_str());

    // Add an active, non-primary profile.
    const auto data2 = contact_data_.AddSocialMediaProfile(
        "activeProfileValue", claim::ClaimType::Facebook, false, true);
    // Verify it is the best profile.
    ASSERT_STREQ(
        "activeProfileValue",
        data2.BestSocialMediaProfile(claim::ClaimType::Facebook).c_str());

    // Add an active profile to a contact data with a primary profile (data1).
    const auto data3 = data1.AddSocialMediaProfile(
        "activeProfileValue", claim::ClaimType::Facebook, false, true);
    // Verify the primary profile is the best.
    ASSERT_STREQ(
        "profileValue",
        data3.BestSocialMediaProfile(claim::ClaimType::Facebook).c_str());

    // Add a new primary profile.
    const auto data4 = data3.AddSocialMediaProfile(
        "primaryProfileValue", claim::ClaimType::Facebook, true, false);
    // Verify it is the best profile.
    ASSERT_STREQ(
        "primaryProfileValue",
        data4.BestSocialMediaProfile(claim::ClaimType::Facebook).c_str());
}

TEST_F(ContactData, Claim_found)
{
    const auto data1 = contact_data_.AddItem(active_contact_item_);
    ASSERT_NE(nullptr, data1.Claim(active_contact_item_->ID()));
}

TEST_F(ContactData, Claim_not_found)
{
    ASSERT_FALSE(contact_data_.Claim(active_contact_item_->ID()));
}

TEST_F(ContactData, Contracts)
{
    const auto data1 = contact_data_.AddContract(
        "instrumentDefinitionID1", ot::UnitType::Usd, false, false);
    const auto contracts = data1.Contracts(ot::UnitType::Usd, false);
    ASSERT_EQ(1, contracts.size());
}

TEST_F(ContactData, Contracts_onlyactive)
{
    const auto data1 = contact_data_.AddContract(
        "instrumentDefinitionID1", ot::UnitType::Usd, false, true);
    const auto data2 = data1.AddContract(
        "instrumentDefinitionID2", ot::UnitType::Usd, false, false);
    const auto contracts = data2.Contracts(ot::UnitType::Usd, true);
    ASSERT_EQ(1, contracts.size());
}

TEST_F(ContactData, Delete)
{
    const auto data1 = contact_data_.AddItem(active_contact_item_);
    const auto contactItem2 = std::make_shared<claim::Item>(
        claim_to_contact_item(client_1_.Factory().Claim(
            nym_id_1_,
            claim::SectionType::Identifier,
            claim::ClaimType::Employee,
            "contactItemValue2",
            active_attr_)));
    const auto data2 = data1.AddItem(contactItem2);

    const auto data3 = data2.Delete(active_contact_item_->ID());
    // Verify the item was deleted.
    ASSERT_EQ(1, data3.Section(claim::SectionType::Identifier)->Size());
    ASSERT_FALSE(data3.Claim(active_contact_item_->ID()));

    const auto data4 = data3.Delete(active_contact_item_->ID());
    // Verify trying to delete the item again didn't change anything.
    ASSERT_EQ(1, data4.Section(claim::SectionType::Identifier)->Size());

    const auto data5 = data4.Delete(contactItem2->ID());
    // Verify the section was removed.
    ASSERT_FALSE(data5.Section(claim::SectionType::Identifier));
}

TEST_F(ContactData, EmailAddresses)
{
    const auto data2 = contact_data_.AddEmail("email1", true, false);
    const auto data3 = data2.AddEmail("email2", false, true);
    const auto data4 = data3.AddEmail("email3", false, false);

    auto emails = data4.EmailAddresses(false);
    ASSERT_TRUE(
        emails.find("email1") != ot::UnallocatedCString::npos &&
        emails.find("email2") != ot::UnallocatedCString::npos &&
        emails.find("email3") != ot::UnallocatedCString::npos);

    emails = data4.EmailAddresses();
    ASSERT_TRUE(
        emails.find("email1") != ot::UnallocatedCString::npos &&
        emails.find("email2") != ot::UnallocatedCString::npos);
    ASSERT_TRUE(emails.find("email3") == ot::UnallocatedCString::npos);
}

TEST_F(ContactData, Group_found)
{
    const auto data1 = contact_data_.AddItem(active_contact_item_);
    ASSERT_NE(
        nullptr,
        data1.Group(
            claim::SectionType::Identifier, claim::ClaimType::Employee));
}

TEST_F(ContactData, Group_notfound)
{
    ASSERT_FALSE(contact_data_.Group(
        claim::SectionType::Identifier, claim::ClaimType::Employee));
}

TEST_F(ContactData, HaveClaim_1)
{
    ASSERT_FALSE(contact_data_.HaveClaim(active_contact_item_->ID()));

    const auto data1 = contact_data_.AddItem(active_contact_item_);
    ASSERT_TRUE(data1.HaveClaim(active_contact_item_->ID()));
}

TEST_F(ContactData, HaveClaim_2)
{
    // Test for an item in group that doesn't exist.
    ASSERT_FALSE(contact_data_.HaveClaim(
        claim::SectionType::Identifier,
        claim::ClaimType::Employee,
        "activeContactItemValue"));

    // Test for an item that does exist.
    const auto data1 = contact_data_.AddItem(active_contact_item_);
    ASSERT_TRUE(data1.HaveClaim(
        claim::SectionType::Identifier,
        claim::ClaimType::Employee,
        "activeContactItemValue"));

    // Test for an item that doesn't exist in a group that does.
    ASSERT_FALSE(data1.HaveClaim(
        claim::SectionType::Identifier,
        claim::ClaimType::Employee,
        "dummyContactItemValue"));
}

TEST_F(ContactData, Name)
{
    // Verify that Name returns an empty string if there is no scope group.
    ASSERT_STREQ("", contact_data_.Name().c_str());

    // Test when the scope group is emtpy.
    const auto group1 = std::make_shared<claim::Group>(
        "contactGroup1",
        claim::SectionType::Scope,
        claim::ClaimType::Individual,
        claim::Group::ItemMap{});

    const auto section1 = std::make_shared<claim::Section>(
        client_1_,
        "contactSectionNym1",
        opentxs::identity::wot::claim::DefaultVersion(),
        opentxs::identity::wot::claim::DefaultVersion(),
        claim::SectionType::Scope,
        claim::Section::GroupMap{{claim::ClaimType::Individual, group1}});

    const claim::Data data1(
        client_1_,
        nym_id_2_.asBase58(client_1_.Crypto()),
        opentxs::identity::wot::claim::DefaultVersion(),
        opentxs::identity::wot::claim::DefaultVersion(),
        claim::Data::SectionMap{{claim::SectionType::Scope, section1}});
    // Verify that Name returns an empty string.
    ASSERT_STREQ("", data1.Name().c_str());

    // Test when the scope is set.
    const auto data2 = contact_data_.SetScope(
        claim::ClaimType::Individual, "activeContactItemValue");
    ASSERT_STREQ("activeContactItemValue", data2.Name().c_str());
}

TEST_F(ContactData, PhoneNumbers)
{
    const auto data2 =
        contact_data_.AddPhoneNumber("phonenumber1", true, false);
    const auto data3 = data2.AddPhoneNumber("phonenumber2", false, true);
    const auto data4 = data3.AddPhoneNumber("phonenumber3", false, false);

    auto phonenumbers = data4.PhoneNumbers(false);
    ASSERT_TRUE(
        phonenumbers.find("phonenumber1") != ot::UnallocatedCString::npos &&
        phonenumbers.find("phonenumber2") != ot::UnallocatedCString::npos &&
        phonenumbers.find("phonenumber3") != ot::UnallocatedCString::npos);

    phonenumbers = data4.PhoneNumbers();
    ASSERT_TRUE(
        phonenumbers.find("phonenumber1") != ot::UnallocatedCString::npos &&
        phonenumbers.find("phonenumber2") != ot::UnallocatedCString::npos);
    ASSERT_TRUE(
        phonenumbers.find("phonenumber3") == ot::UnallocatedCString::npos);
}

TEST_F(ContactData, PreferredOTServer)
{
    // Test getting the preferred server with no group.
    const auto identifier = contact_data_.PreferredOTServer();
    ASSERT_TRUE(identifier.empty());

    // Test getting the preferred server with an empty group.
    const auto group1 = std::make_shared<claim::Group>(
        "contactGroup1",
        claim::SectionType::Communication,
        claim::ClaimType::Opentxs,
        claim::Group::ItemMap{});

    const auto section1 = std::make_shared<claim::Section>(
        client_1_,
        "contactSectionNym1",
        opentxs::identity::wot::claim::DefaultVersion(),
        opentxs::identity::wot::claim::DefaultVersion(),
        claim::SectionType::Communication,
        claim::Section::GroupMap{{claim::ClaimType::Opentxs, group1}});

    const claim::Data data1(
        client_1_,
        nym_id_2_.asBase58(client_1_.Crypto()),
        opentxs::identity::wot::claim::DefaultVersion(),
        opentxs::identity::wot::claim::DefaultVersion(),
        claim::Data::SectionMap{{claim::SectionType::Communication, section1}});

    const auto identifier2 = data1.PreferredOTServer();
    ASSERT_TRUE(identifier2.empty());

    // Test getting the preferred server.
    const auto serverIdentifier2 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_1_,
        claim::SectionType::Communication,
        claim::ClaimType::Opentxs,
        {},
        {},
        "serverID2"s,
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto data2 =
        contact_data_.AddPreferredOTServer(serverIdentifier2, true);
    const auto preferredServer = data2.PreferredOTServer();
    ASSERT_FALSE(preferredServer.empty());
    ASSERT_EQ(serverIdentifier2, preferredServer);
}

TEST_F(ContactData, Section)
{
    ASSERT_FALSE(contact_data_.Section(claim::SectionType::Identifier));

    const auto data1 = contact_data_.AddItem(active_contact_item_);
    ASSERT_NE(nullptr, data1.Section(claim::SectionType::Identifier));
}

TEST_F(ContactData, SetCommonName)
{
    const auto data1 = contact_data_.SetCommonName("commonName");
    const auto identifier = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_1_,
        claim::SectionType::Identifier,
        claim::ClaimType::Commonname,
        {},
        {},
        "commonName"s,
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto commonNameItem = data1.Claim(identifier);
    ASSERT_NE(nullptr, commonNameItem);
    ASSERT_TRUE(commonNameItem->HasAttribute(
        opentxs::identity::wot::claim::Attribute::Primary));
    ASSERT_TRUE(commonNameItem->HasAttribute(
        opentxs::identity::wot::claim::Attribute::Active));
}

TEST_F(ContactData, SetName)
{
    const auto data1 =
        contact_data_.SetScope(claim::ClaimType::Individual, "firstName");

    // Test that SetName creates a scope item.
    const auto data2 = data1.SetName("secondName");
    // Verify the item was created in the scope section and made primary.
    const auto identifier1 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_1_,
        claim::SectionType::Scope,
        claim::ClaimType::Individual,
        {},
        {},
        "secondName"s,
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto scopeItem1 = data2.Claim(identifier1);
    ASSERT_NE(nullptr, scopeItem1);
    ASSERT_TRUE(scopeItem1->HasAttribute(
        opentxs::identity::wot::claim::Attribute::Primary));
    ASSERT_TRUE(scopeItem1->HasAttribute(
        opentxs::identity::wot::claim::Attribute::Active));

    // Test that SetName creates an item in the scope section without making it
    // primary.
    const auto data3 = data2.SetName("thirdName", false);
    const auto identifier2 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_1_,
        claim::SectionType::Scope,
        claim::ClaimType::Individual,
        {},
        {},
        "thirdName"s,
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto contactItem2 = data3.Claim(identifier2);
    ASSERT_NE(nullptr, contactItem2);
    ASSERT_FALSE(contactItem2->HasAttribute(
        opentxs::identity::wot::claim::Attribute::Primary));
    ASSERT_TRUE(contactItem2->HasAttribute(
        opentxs::identity::wot::claim::Attribute::Active));
}

TEST_F(ContactData, SetScope)
{
    const auto data1 = contact_data_.SetScope(
        claim::ClaimType::Organization, "organizationScope");
    // Verify the scope item was created.
    const auto identifier1 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_1_,
        claim::SectionType::Scope,
        claim::ClaimType::Organization,
        {},
        {},
        "organizationScope"s,
        "",
        ot::identity::wot::claim::DefaultVersion());
    const auto scopeItem1 = data1.Claim(identifier1);
    ASSERT_NE(nullptr, scopeItem1);
    ASSERT_TRUE(scopeItem1->HasAttribute(
        opentxs::identity::wot::claim::Attribute::Primary));
    ASSERT_TRUE(scopeItem1->HasAttribute(
        opentxs::identity::wot::claim::Attribute::Active));

    // Test when there is an existing scope.
    const auto data2 =
        data1.SetScope(claim::ClaimType::Organization, "businessScope");
    // Verify the item wasn't added.
    const auto identifier2 = ot::identity::credential::Contact::ClaimID(
        client_1_,
        nym_id_1_,
        claim::SectionType::Scope,
        claim::ClaimType::Business,
        {},
        {},
        "businessScope"s,
        "",
        ot::identity::wot::claim::DefaultVersion());
    ASSERT_FALSE(data2.Claim(identifier2));
    // Verify the scope wasn't changed.
    const auto scopeItem2 = data2.Claim(identifier1);
    ASSERT_NE(nullptr, scopeItem2);
    ASSERT_TRUE(scopeItem2->HasAttribute(
        opentxs::identity::wot::claim::Attribute::Primary));
    ASSERT_TRUE(scopeItem2->HasAttribute(
        opentxs::identity::wot::claim::Attribute::Active));
}

TEST_F(ContactData, SetScope_different_versions)
{
    const claim::Data data1(
        client_1_,
        "dataNym1"s,
        3,  // version of CONTACTSECTION_SCOPE section before CITEMTYPE_BOT
            // was added
        3,
        {});

    const auto data2 = data1.SetScope(claim::ClaimType::Bot, "botScope");

    ASSERT_EQ(4, data2.Version());
}

TEST_F(ContactData, SocialMediaProfiles)
{
    const auto data2 = contact_data_.AddSocialMediaProfile(
        "facebook1", claim::ClaimType::Facebook, true, false);
    const auto data3 = data2.AddSocialMediaProfile(
        "linkedin1", claim::ClaimType::Linkedin, false, true);
    const auto data4 = data3.AddSocialMediaProfile(
        "facebook2", claim::ClaimType::Facebook, false, false);

    auto profiles =
        data4.SocialMediaProfiles(claim::ClaimType::Facebook, false);
    ASSERT_TRUE(
        profiles.find("facebook1") != ot::UnallocatedCString::npos &&
        profiles.find("facebook2") != ot::UnallocatedCString::npos);

    profiles = data4.SocialMediaProfiles(claim::ClaimType::Linkedin, false);
    ASSERT_STREQ("linkedin1", profiles.c_str());

    profiles = data4.SocialMediaProfiles(claim::ClaimType::Facebook);
    ASSERT_STREQ("facebook1", profiles.c_str());
    ASSERT_TRUE(profiles.find("facebook2") == ot::UnallocatedCString::npos);
    ASSERT_TRUE(profiles.find("linkedin1") == ot::UnallocatedCString::npos);
}

TEST_F(ContactData, Type)
{
    ASSERT_EQ(claim::ClaimType::Unknown, contact_data_.Type());

    const auto data1 =
        contact_data_.SetScope(claim::ClaimType::Individual, "scopeName");
    ASSERT_EQ(claim::ClaimType::Individual, data1.Type());
}

TEST_F(ContactData, Version)
{
    ASSERT_EQ(
        opentxs::identity::wot::claim::DefaultVersion(),
        contact_data_.Version());
}
}  // namespace ottest
