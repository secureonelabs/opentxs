// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "blockchain/block/transaction/TransactionPrivate.hpp"

#include "internal/blockchain/protocol/bitcoin/base/block/Transaction.hpp"
#include "internal/util/PMR.hpp"
#include "opentxs/blockchain/protocol/bitcoin/base/block/Transaction.hpp"
#include "opentxs/util/Allocator.hpp"

namespace opentxs::blockchain::protocol::bitcoin::base::block
{
class TransactionPrivate : virtual public blockchain::block::TransactionPrivate,
                           virtual public internal::Transaction
{
public:
    [[nodiscard]] static auto Blank(allocator_type alloc) noexcept
        -> TransactionPrivate*
    {
        return pmr::default_construct<TransactionPrivate>({alloc});
    }

    auto asBitcoinPrivate() const noexcept
        -> const protocol::bitcoin::base::block::TransactionPrivate* final
    {
        return this;
    }
    auto asBitcoinPublic() const noexcept
        -> const protocol::bitcoin::base::block::Transaction& final
    {
        return self_;
    }
    [[nodiscard]] auto clone(allocator_type alloc) const noexcept
        -> blockchain::block::TransactionPrivate* override
    {
        return pmr::clone_as<blockchain::block::TransactionPrivate>(
            this, {alloc});
    }

    auto asBitcoinPrivate() noexcept
        -> protocol::bitcoin::base::block::TransactionPrivate* final
    {
        return this;
    }
    auto asBitcoinPublic() noexcept
        -> protocol::bitcoin::base::block::Transaction& final
    {
        return self_;
    }
    [[nodiscard]] auto get_deleter() noexcept -> delete_function override
    {
        return pmr::make_deleter(this);
    }

    TransactionPrivate(allocator_type alloc) noexcept;
    TransactionPrivate() = delete;
    TransactionPrivate(
        const TransactionPrivate& rhs,
        allocator_type alloc) noexcept;
    TransactionPrivate(const TransactionPrivate&) = delete;
    TransactionPrivate(TransactionPrivate&&) = delete;
    auto operator=(const TransactionPrivate&) -> TransactionPrivate& = delete;
    auto operator=(TransactionPrivate&&) -> TransactionPrivate& = delete;

    ~TransactionPrivate() override;

private:
    block::Transaction self_;
};
}  // namespace opentxs::blockchain::protocol::bitcoin::base::block
