// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/identifier/Generic.hpp"
// IWYU pragma: no_include <iosfwd>

#pragma once

#include "interface/ui/base/Row.hpp"
#include "internal/interface/ui/UI.hpp"
#include "internal/util/SharedPimpl.hpp"
#include "opentxs/Time.hpp"
#include "opentxs/core/Amount.hpp"
#include "opentxs/identifier/Account.hpp"
#include "opentxs/identifier/Nym.hpp"
#include "opentxs/otx/client/Types.hpp"
#include "opentxs/util/Container.hpp"

class QVariant;

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs
{
namespace api
{
namespace session
{
class Client;
}  // namespace session
}  // namespace api

namespace protobuf
{
class PaymentWorkflow;
}  // namespace protobuf

namespace ui
{
class BalanceItem;
}  // namespace ui
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::ui::implementation
{
using BalanceItemRow =
    Row<AccountActivityRowInternal,
        AccountActivityInternalInterface,
        AccountActivityRowID>;

class BalanceItem : public BalanceItemRow
{
public:
    const api::session::Client& api_;

    static auto recover_workflow(CustomData& custom) noexcept
        -> const protobuf::PaymentWorkflow&;

    auto Confirmations() const noexcept -> int override { return 1; }
    auto Contacts() const noexcept
        -> UnallocatedVector<UnallocatedCString> override
    {
        return contacts_;
    }
    auto DisplayAmount() const noexcept -> UnallocatedCString override;
    auto Text() const noexcept -> UnallocatedCString override;
    auto Timestamp() const noexcept -> Time final;
    auto Type() const noexcept -> otx::client::StorageBox override
    {
        return type_;
    }

    BalanceItem(const BalanceItem&) = delete;
    BalanceItem(BalanceItem&&) = delete;
    auto operator=(const BalanceItem&) -> BalanceItem& = delete;
    auto operator=(BalanceItem&&) -> BalanceItem& = delete;

    ~BalanceItem() override;

protected:
    const identifier::Nym nym_id_;
    const UnallocatedCString workflow_;
    const otx::client::StorageBox type_;
    UnallocatedCString text_;
    Time time_;

    static auto extract_type(const protobuf::PaymentWorkflow& workflow) noexcept
        -> otx::client::StorageBox;

    auto get_contact_name(const identifier::Nym& nymID) const noexcept
        -> UnallocatedCString;

    auto reindex(
        const implementation::AccountActivitySortKey& key,
        implementation::CustomData& custom) noexcept -> bool override;

    BalanceItem(
        const AccountActivityInternalInterface& parent,
        const api::session::Client& api,
        const AccountActivityRowID& rowID,
        const AccountActivitySortKey& sortKey,
        CustomData& custom,
        const identifier::Nym& nymID,
        const identifier::Account& accountID,
        const UnallocatedCString& text = {}) noexcept;

private:
    const identifier::Account account_id_;
    const UnallocatedVector<UnallocatedCString> contacts_;

    static auto extract_contacts(
        const api::session::Client& api,
        const protobuf::PaymentWorkflow& workflow) noexcept
        -> UnallocatedVector<UnallocatedCString>;

    virtual auto effective_amount() const noexcept -> opentxs::Amount = 0;
    auto qt_data(const int column, const int role, QVariant& out) const noexcept
        -> void final;
};
}  // namespace opentxs::ui::implementation

template class opentxs::SharedPimpl<opentxs::ui::BalanceItem>;
