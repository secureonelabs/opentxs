// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "blockchain/protocol/bitcoin/base/block/parser/Base.hpp"  // IWYU pragma: associated

#include <algorithm>  // IWYU pragma: keep
#include <execution>

namespace opentxs::blockchain::protocol::bitcoin::base::block
{
auto ParserBase::get_transactions(std::span<Data> view) const noexcept -> void
{
    using namespace std::execution;
    std::for_each(
        par, view.begin(), view.end(), [this](auto& t) { get_transaction(t); });
}
}  // namespace opentxs::blockchain::protocol::bitcoin::base::block
