// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_forward_declare opentxs::protobuf::ContactItemAttribute

#include "identity/credential/Contact.hpp"  // IWYU pragma: associated

#include <opentxs/protobuf/ContactData.pb.h>
#include <opentxs/protobuf/Credential.pb.h>
#include <opentxs/protobuf/Signature.pb.h>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>

#include "identity/credential/Base.hpp"
#include "internal/crypto/Parameters.hpp"
#include "internal/crypto/key/Key.hpp"
#include "opentxs/Time.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/api/Session.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/crypto/Parameters.hpp"
#include "opentxs/crypto/asymmetric/Mode.hpp"  // IWYU pragma: keep
#include "opentxs/crypto/asymmetric/Types.hpp"
#include "opentxs/identifier/Generic.hpp"
#include "opentxs/identity/CredentialRole.hpp"  // IWYU pragma: keep
#include "opentxs/identity/Types.hpp"
#include "opentxs/identity/credential/Contact.hpp"
#include "opentxs/identity/wot/Claim.hpp"
#include "opentxs/identity/wot/Types.hpp"
#include "opentxs/identity/wot/claim/Types.hpp"
#include "opentxs/internal.factory.hpp"
#include "opentxs/util/Log.hpp"

namespace opentxs
{
auto Factory::ContactCredential(
    const api::Session& api,
    identity::internal::Authority& parent,
    const identity::Source& source,
    const identity::credential::internal::Primary& master,
    const crypto::Parameters& parameters,
    const VersionNumber version,
    const opentxs::PasswordPrompt& reason)
    -> identity::credential::internal::Contact*
{
    using ReturnType = identity::credential::implementation::Contact;

    try {

        return new ReturnType(
            api, parent, source, master, parameters, version, reason);
    } catch (const std::exception& e) {
        LogError()()("Failed to create credential: ")(e.what()).Flush();

        return nullptr;
    }
}

auto Factory::ContactCredential(
    const api::Session& api,
    identity::internal::Authority& parent,
    const identity::Source& source,
    const identity::credential::internal::Primary& master,
    const protobuf::Credential& serialized)
    -> identity::credential::internal::Contact*
{
    using ReturnType = identity::credential::implementation::Contact;

    try {

        return new ReturnType(api, parent, source, master, serialized);
    } catch (const std::exception& e) {
        LogError()()("Failed to deserialize credential: ")(e.what()).Flush();

        return nullptr;
    }
}
}  // namespace opentxs

namespace opentxs::identity::credential
{
auto Contact::ClaimID(
    const api::Session& api,
    const identity::wot::Claimant& claimant,
    const wot::claim::SectionType section,
    const wot::claim::ClaimType type,
    Time start,
    Time end,
    std::string_view value,
    ReadView subtype,
    VersionNumber version) -> identifier::Generic
{
    const auto claim = api.Factory().Claim(
        claimant, section, type, value, {}, start, end, subtype, version);

    return claim.ID();
}
}  // namespace opentxs::identity::credential

namespace opentxs::identity::credential::implementation
{
Contact::Contact(
    const api::Session& api,
    const identity::internal::Authority& parent,
    const identity::Source& source,
    const internal::Primary& master,
    const crypto::Parameters& params,
    const VersionNumber version,
    const PasswordPrompt& reason) noexcept(false)
    : credential::implementation::Base(
          api,
          parent,
          source,
          params,
          version,
          identity::CredentialRole::Contact,
          crypto::asymmetric::Mode::Null,
          master.ID())
    , data_(
          [&](const crypto::Parameters& parameters)
              -> const protobuf::ContactData {
              auto proto = protobuf::ContactData{};
              parameters.Internal().GetContactData(proto);
              return proto;
          }(params))
{
    first_time_init(set_name_from_id_);
    init(master, reason);
}

Contact::Contact(
    const api::Session& api,
    const identity::internal::Authority& parent,
    const identity::Source& source,
    const internal::Primary& master,
    const protobuf::Credential& serialized) noexcept(false)
    : credential::implementation::Base(
          api,
          parent,
          source,
          serialized,
          get_master_id(api, serialized, master))
    , data_(serialized.contactdata())
{
    init_serialized();
}

auto Contact::GetContactData(protobuf::ContactData& contactData) const -> bool
{
    contactData = protobuf::ContactData(data_);

    return true;
}

auto Contact::id_form() const -> std::shared_ptr<SerializedType>
{
    auto out = Base::id_form();
    out->set_mode(translate(crypto::asymmetric::Mode::Null));
    *out->mutable_contactdata() = data_;

    return out;
}

auto Contact::serialize(
    const SerializationModeFlag asPrivate,
    const SerializationSignatureFlag asSigned) const
    -> std::shared_ptr<Base::SerializedType>
{
    auto out = Base::serialize(asPrivate, asSigned);

    if (asSigned) {
        if (auto sig = MasterSignature(); sig) {
            *out->add_signature() = *sig;
        } else {
            LogError()()("Failed to get master signature.").Flush();
        }
    }

    return out;
}
}  // namespace opentxs::identity::credential::implementation
