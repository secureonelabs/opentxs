// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include <string_view>
// IWYU pragma: no_include <typeindex>

#pragma once

#include <cstddef>

#include "opentxs/Export.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/core/FixedByteArray.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs
{
namespace blockchain
{
namespace cfilter
{
class Hash;
}  // namespace cfilter
}  // namespace blockchain
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace std
{
template <>
struct OPENTXS_EXPORT hash<opentxs::blockchain::cfilter::Hash> {
    using is_transparent = void;
    using is_avalanching = void;

    auto operator()(const opentxs::blockchain::cfilter::Hash& data)
        const noexcept -> std::size_t;
};
}  // namespace std

namespace opentxs::blockchain::cfilter
{
class OPENTXS_IMPORT Hash : virtual public FixedByteArray<32>
{
public:
    Hash() noexcept;
    Hash(const ReadView bytes) noexcept(false);
    Hash(const Hash& rhs) noexcept;
    auto operator=(const Hash& rhs) noexcept -> Hash&;

    ~Hash() override;
};
}  // namespace opentxs::blockchain::cfilter
