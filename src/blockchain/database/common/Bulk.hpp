// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <filesystem>

#include "internal/util/PMR.hpp"
#include "internal/util/storage/file/Mapped.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs
{
namespace storage
{
namespace lmdb
{
class Database;
}  // namespace lmdb
}  // namespace storage
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::blockchain::database::common
{
class Bulk final : public storage::file::Mapped
{
public:
    auto get_deleter() noexcept -> delete_function final
    {
        return pmr::make_deleter(this);
    }

    Bulk(
        storage::lmdb::Database& lmdb,
        const std::filesystem::path& path) noexcept(false);
    Bulk() = delete;
    Bulk(const Bulk&) = delete;
    Bulk(Bulk&&) = delete;
    auto operator=(const Bulk&) -> Bulk& = delete;
    auto operator=(Bulk&&) -> Bulk& = delete;

    ~Bulk() final;
};
}  // namespace opentxs::blockchain::database::common
