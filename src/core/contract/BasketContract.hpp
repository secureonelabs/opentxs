// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <opentxs/protobuf/UnitDefinition.pb.h>
#include <cstdint>

#include "core/contract/Unit.hpp"
#include "internal/core/contract/BasketContract.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/contract/Types.hpp"
#include "opentxs/contract/UnitDefinitionType.hpp"  // IWYU pragma: keep
#include "opentxs/core/Amount.hpp"
#include "opentxs/identity/Types.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Numbers.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs
{
namespace api
{
class Session;
}  // namespace api

namespace display
{
class Definition;
}  // namespace display
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::contract::unit::implementation
{
class Basket final : public unit::Basket, public contract::implementation::Unit
{
public:
    auto BasketID() const -> identifier_type final;
    auto Currencies() const -> const Subcontracts& final
    {
        return subcontracts_;
    }
    auto Type() const -> contract::UnitDefinitionType final
    {
        return contract::UnitDefinitionType::Basket;
    }
    auto Weight() const -> std::uint64_t final { return weight_; }

    Basket(
        const api::Session& api,
        const Nym_p& nym,
        const UnallocatedCString& shortname,
        const UnallocatedCString& terms,
        const std::uint64_t weight,
        const opentxs::UnitType unitOfAccount,
        const VersionNumber version,
        const display::Definition& displayDefinition,
        const Amount& redemptionIncrement);
    Basket(
        const api::Session& api,
        const Nym_p& nym,
        const protobuf::UnitDefinition serialized);
    Basket(Basket&&) = delete;
    auto operator=(const Basket&) -> Basket& = delete;
    auto operator=(Basket&&) -> Basket& = delete;

    ~Basket() final = default;

private:
    friend unit::Basket;

    const Subcontracts subcontracts_;
    const std::uint64_t weight_;

    auto BasketIDVersion() const -> protobuf::UnitDefinition;
    auto IDVersion() const -> protobuf::UnitDefinition final;

    Basket(const Basket&);
};
}  // namespace opentxs::contract::unit::implementation
