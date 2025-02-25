// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "internal/blockchain/block/Block.hpp"
#include "internal/util/PMR.hpp"
#include "internal/util/alloc/Allocated.hpp"
#include "opentxs/util/Allocator.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs
{
namespace blockchain
{
namespace block
{
class Block;
}  // namespace block

namespace protocol
{
namespace bitcoin
{
namespace base
{
namespace block
{
class Block;
class BlockPrivate;
}  // namespace block
}  // namespace base
}  // namespace bitcoin
}  // namespace protocol
}  // namespace blockchain
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::block
{
class BlockPrivate : virtual public internal::Block,
                     public opentxs::pmr::Allocated
{
public:
    [[nodiscard]] static auto Blank(allocator_type alloc) noexcept
        -> BlockPrivate*
    {
        return pmr::default_construct<BlockPrivate>({alloc});
    }
    static auto Reset(block::Block& header) noexcept -> void;

    virtual auto asBitcoinPrivate() const noexcept
        -> const protocol::bitcoin::base::block::BlockPrivate*;
    virtual auto asBitcoinPublic() const noexcept
        -> const protocol::bitcoin::base::block::Block&;
    [[nodiscard]] virtual auto clone(allocator_type alloc) const noexcept
        -> BlockPrivate*
    {
        return pmr::clone(this, {alloc});
    }

    virtual auto asBitcoinPrivate() noexcept
        -> protocol::bitcoin::base::block::BlockPrivate*;
    virtual auto asBitcoinPublic() noexcept
        -> protocol::bitcoin::base::block::Block&;
    [[nodiscard]] auto get_deleter() noexcept -> delete_function override
    {
        return pmr::make_deleter(this);
    }

    BlockPrivate(allocator_type alloc) noexcept;
    BlockPrivate() = delete;
    BlockPrivate(const BlockPrivate& rhs, allocator_type alloc) noexcept;
    BlockPrivate(const BlockPrivate&) = delete;
    BlockPrivate(BlockPrivate&&) = delete;
    auto operator=(const BlockPrivate&) -> BlockPrivate& = delete;
    auto operator=(BlockPrivate&&) -> BlockPrivate& = delete;

    ~BlockPrivate() override;
};
}  // namespace opentxs::blockchain::block
