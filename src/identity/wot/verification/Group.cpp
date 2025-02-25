// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "identity/wot/verification/Group.hpp"  // IWYU pragma: associated

#include <opentxs/protobuf/VerificationGroup.pb.h>
#include <opentxs/protobuf/VerificationIdentity.pb.h>
#include <opentxs/protobuf/VerificationItem.pb.h>
#include <stdexcept>
#include <utility>

#include "opentxs/core/Data.hpp"
#include "opentxs/identifier/Nym.hpp"
#include "opentxs/identity/wot/verification/Group.hpp"
#include "opentxs/identity/wot/verification/Nym.hpp"
#include "opentxs/internal.factory.hpp"
#include "opentxs/protobuf/Types.internal.hpp"
#include "opentxs/protobuf/syntax/VerifyContacts.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"

namespace opentxs
{
auto Factory::VerificationGroup(
    identity::wot::verification::internal::Set& parent,
    const VersionNumber version,
    bool external) -> identity::wot::verification::internal::Group*
{
    using ReturnType =
        opentxs::identity::wot::verification::implementation::Group;

    try {

        return new ReturnType(parent, external, version);
    } catch (const std::exception& e) {
        LogError()()("Failed to construct verification nym: ")(e.what())
            .Flush();

        return nullptr;
    }
}

auto Factory::VerificationGroup(
    identity::wot::verification::internal::Set& parent,
    const protobuf::VerificationGroup& serialized,
    bool external) -> identity::wot::verification::internal::Group*
{
    using ReturnType =
        opentxs::identity::wot::verification::implementation::Group;

    try {

        return new ReturnType(parent, serialized, external);
    } catch (const std::exception& e) {
        LogError()()("Failed to construct verification nym: ")(e.what())
            .Flush();

        return nullptr;
    }
}
}  // namespace opentxs

namespace opentxs::identity::wot::verification
{
const VersionNumber Group::DefaultVersion{1};
}

namespace opentxs::identity::wot::verification::implementation
{
Group::Group(
    internal::Set& parent,
    bool external,
    const VersionNumber version) noexcept
    : parent_(parent)
    , version_(version)
    , external_(external)
    , nyms_()
    , map_()
{
}

Group::Group(
    internal::Set& parent,
    const SerializedType& in,
    bool external) noexcept
    : parent_(parent)
    , version_(in.version())
    , external_(external)
    , nyms_(instantiate(*this, in))
    , map_()
{
}

Group::operator SerializedType() const noexcept
{
    auto output = SerializedType{};
    output.set_version(version_);

    for (const auto& pNym : nyms_) {
        assert_false(nullptr == pNym);

        const auto& nym = *pNym;
        output.add_identity()->CopyFrom(nym);
    }

    return output;
}

auto Group::AddItem(
    const identifier::Nym& claimOwner,
    const identifier::Generic& claim,
    const identity::Nym& signer,
    const PasswordPrompt& reason,
    const verification::Type value,
    const Time start,
    const Time end,
    const VersionNumber version) noexcept -> bool
{
    if (external_) {
        LogError()()("Invalid internal item").Flush();

        return false;
    }

    return get_nym(claimOwner)
        .AddItem(claim, signer, reason, value, start, end, version);
}

auto Group::AddItem(
    const identifier::Nym& verifier,
    const internal::Item::SerializedType verification) noexcept -> bool
{
    if (false == external_) {
        LogError()()("Invalid external item").Flush();

        return false;
    }

    if (verifier == parent_.NymID()) {
        LogError()()("Attempting to add internal claim to external section")
            .Flush();

        return false;
    }

    return get_nym(verifier).AddItem(verification);
}

auto Group::DeleteItem(const identifier::Generic& item) noexcept -> bool
{
    auto it = map_.find(item);

    if (map_.end() == it) { return false; }

    return get_nym(it->second).DeleteItem(item);
}

auto Group::get_nym(const identifier::Nym& id) noexcept -> internal::Nym&
{
    for (auto& pNym : nyms_) {
        assert_false(nullptr == pNym);

        auto& nym = *pNym;

        if (id == nym.ID()) { return nym; }
    }

    auto pNym = std::unique_ptr<internal::Nym>{
        Factory::VerificationNym(*this, id, Nym::DefaultVersion)};

    assert_false(nullptr == pNym);

    nyms_.emplace_back(std::move(pNym));

    return **nyms_.rbegin();
}

auto Group::instantiate(
    internal::Group& parent,
    const SerializedType& in) noexcept -> Vector
{
    auto output = Vector{};

    for (const auto& serialized : in.identity()) {
        auto pItem = std::unique_ptr<internal::Nym>{
            Factory::VerificationNym(parent, serialized)};

        if (pItem) { output.emplace_back(std::move(pItem)); }
    }

    return output;
}

auto Group::Register(
    const identifier::Generic& id,
    const identifier::Nym& nym) noexcept -> void
{
    parent_.Register(id, external_);
    auto it = map_.find(id);

    if (map_.end() == it) {
        map_.emplace(id, nym);
    } else {
        it->second = nym;
    }
}

auto Group::Unregister(const identifier::Generic& id) noexcept -> void
{
    parent_.Unregister(id);
    map_.erase(id);
}

auto Group::UpgradeNymVersion(const VersionNumber nymVersion) noexcept -> bool
{
    auto groupVersion{version_};

    try {
        while (true) {
            const auto [min, max] =
                protobuf::VerificationGroupAllowedIdentity().at(groupVersion);

            if (nymVersion < min) {
                LogError()()("Version ")(nymVersion)(" too old").Flush();

                return false;
            }

            if (nymVersion > max) {
                ++groupVersion;
            } else {
                if (false == parent_.UpgradeGroupVersion(groupVersion)) {
                    return false;
                }

                return true;
            }
        }
    } catch (...) {
        LogError()()("No support for version ")(nymVersion)(" items").Flush();

        return false;
    }
}
}  // namespace opentxs::identity::wot::verification::implementation
