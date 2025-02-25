// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "internal/interface/ui/List.hpp"
#include "internal/interface/ui/ListRow.hpp"
#include "internal/util/SharedPimpl.hpp"
#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/util/Container.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs
{
namespace ui
{
class BlockchainSubaccount;
}  // namespace ui
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::ui
{
class BlockchainSubaccountSource : virtual public List, virtual public ListRow
{
public:
    virtual auto First() const noexcept
        -> SharedPimpl<BlockchainSubaccount> = 0;
    virtual auto Name() const noexcept -> UnallocatedCString = 0;
    virtual auto Next() const noexcept -> SharedPimpl<BlockchainSubaccount> = 0;
    virtual auto SourceID() const noexcept -> const identifier::Generic& = 0;
    virtual auto Type() const noexcept
        -> blockchain::crypto::SubaccountType = 0;

    BlockchainSubaccountSource(const BlockchainSubaccountSource&) = delete;
    BlockchainSubaccountSource(BlockchainSubaccountSource&&) = delete;
    auto operator=(const BlockchainSubaccountSource&)
        -> BlockchainSubaccountSource& = delete;
    auto operator=(BlockchainSubaccountSource&&)
        -> BlockchainSubaccountSource& = delete;

    ~BlockchainSubaccountSource() override = default;

protected:
    BlockchainSubaccountSource() noexcept = default;
};
}  // namespace opentxs::ui
