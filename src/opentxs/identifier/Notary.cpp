// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "opentxs/identifier/Notary.hpp"  // IWYU pragma: associated

#include <utility>

#include "internal/core/identifier/Factory.hpp"
#include "internal/util/PMR.hpp"
#include "opentxs/identifier/AccountSubtype.hpp"  // IWYU pragma: keep
#include "opentxs/identifier/Type.hpp"            // IWYU pragma: keep
#include "opentxs/identifier/Types.hpp"

namespace opentxs::identifier
{
Notary::Notary(IdentifierPrivate* imp) noexcept
    : Generic(std::move(imp))
{
}

Notary::Notary(allocator_type a) noexcept
    : Generic(factory::Identifier(
          identifier::Type::notary,
          identifier::AccountSubtype::invalid_subtype,
          std::move(a)))
{
}

Notary::Notary(const Notary& rhs, allocator_type alloc) noexcept
    : Generic(rhs, std::move(alloc))
{
}

Notary::Notary(Notary&& rhs) noexcept
    : Generic(std::move(rhs))
{
}

Notary::Notary(Notary&& rhs, allocator_type alloc) noexcept
    : Generic(std::move(rhs), std::move(alloc))
{
}

auto Notary::operator=(const Notary& rhs) noexcept -> Notary&
{
    // NOLINTNEXTLINE(misc-unconventional-assign-operator)
    return pmr::copy_assign_child<Generic>(*this, rhs);
}

auto Notary::operator=(Notary&& rhs) noexcept -> Notary&
{
    // NOLINTNEXTLINE(misc-unconventional-assign-operator)
    return pmr::move_assign_child<Generic>(*this, std::move(rhs));
}

Notary::~Notary() = default;
}  // namespace opentxs::identifier
