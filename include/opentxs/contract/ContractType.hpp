// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <type_traits>

#include "opentxs/contract/Types.hpp"  // IWYU pragma: keep

namespace opentxs::contract
{
enum class Type : std::underlying_type_t<Type> {
    invalid = 0,
    nym = 1,
    notary = 2,
    unit = 3,
};
}  // namespace opentxs::contract
