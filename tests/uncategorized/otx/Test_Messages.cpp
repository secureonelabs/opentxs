// Copyright (c) 2010-2023 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include <opentxs/opentxs.hpp>
#include <memory>

#include "ottest/fixtures/otx/Messages.hpp"

namespace ottest
{

TEST_F(Messages, activateRequest)
{
    const ot::otx::ServerRequestType type{ot::otx::ServerRequestType::Activate};
    auto requestID = ot::identifier::Generic{};
    const auto alice = client_.Wallet().Nym(alice_nym_id_);

    ASSERT_TRUE(alice);

    auto request = ot::otx::Request::Factory(
        client_, alice, server_id_, type, 1, reason_c_);

    ASSERT_TRUE(request.Nym());
    EXPECT_EQ(alice_nym_id_, request.Nym()->ID());
    EXPECT_EQ(alice_nym_id_, request.Initiator());
    EXPECT_EQ(server_id_, request.Server());
    EXPECT_EQ(type, request.Type());
    EXPECT_EQ(1, request.Number());

    requestID = request.ID();

    EXPECT_FALSE(requestID.empty());
    EXPECT_TRUE(request.Validate());

    EXPECT_EQ(ot::otx::Request::DefaultVersion, request.Version());
    EXPECT_EQ(requestID, request.ID());

    request.SetIncludeNym(true, reason_c_);

    EXPECT_TRUE(request.Validate());

    auto bytes = ot::Space{};
    EXPECT_TRUE(request.Serialize(ot::writer(bytes)));

    const auto serverCopy =
        ot::otx::Request::Factory(server_, ot::reader(bytes));

    ASSERT_TRUE(serverCopy.Nym());
    EXPECT_EQ(alice_nym_id_, serverCopy.Nym()->ID());
    EXPECT_EQ(alice_nym_id_, serverCopy.Initiator());
    EXPECT_EQ(server_id_, serverCopy.Server());
    EXPECT_EQ(type, serverCopy.Type());
    EXPECT_EQ(1, serverCopy.Number());
    EXPECT_EQ(requestID, serverCopy.ID());
    EXPECT_TRUE(serverCopy.Validate());
}

TEST_F(Messages, pushReply)
{
    const ot::UnallocatedCString payload{"TEST PAYLOAD"};
    const ot::otx::ServerReplyType type{ot::otx::ServerReplyType::Push};
    auto replyID = ot::identifier::Generic{};
    const auto server = server_.Wallet().Nym(server_.NymID());

    ASSERT_TRUE(server);

    auto reply = ot::otx::Reply::Factory(
        server_,
        server,
        alice_nym_id_,
        server_id_,
        type,
        true,
        true,
        reason_s_,
        ot::otx::PushType::Nymbox,
        payload);

    ASSERT_TRUE(reply.Nym());
    EXPECT_EQ(server_.NymID(), reply.Nym()->ID());
    EXPECT_EQ(alice_nym_id_, reply.Recipient());
    EXPECT_EQ(server_id_, reply.Server());
    EXPECT_EQ(type, reply.Type());
    EXPECT_EQ(1, reply.Number());
    EXPECT_TRUE(reply.Push());

    replyID = reply.ID();

    EXPECT_FALSE(replyID.empty());
    EXPECT_TRUE(reply.Validate());

    auto bytes = ot::Space{};
    EXPECT_TRUE(reply.Serialize(ot::writer(bytes)));

    EXPECT_EQ(ot::otx::Reply::DefaultVersion, reply.Version());
    EXPECT_EQ(
        replyID.asBase58(client_.Crypto()),
        reply.ID().asBase58(client_.Crypto()));

    EXPECT_TRUE(reply.Validate());

    const auto aliceCopy = ot::otx::Reply::Factory(client_, ot::reader(bytes));

    ASSERT_TRUE(aliceCopy.Nym());
    EXPECT_EQ(server_.NymID(), aliceCopy.Nym()->ID());
    EXPECT_EQ(alice_nym_id_, aliceCopy.Recipient());
    EXPECT_EQ(server_id_, aliceCopy.Server());
    EXPECT_EQ(type, aliceCopy.Type());
    EXPECT_EQ(1, aliceCopy.Number());
    EXPECT_EQ(replyID, aliceCopy.ID());
    ASSERT_TRUE(aliceCopy.Push());
    EXPECT_TRUE(aliceCopy.Validate());
}
}  // namespace ottest
