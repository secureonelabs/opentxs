// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "opentxs/interface/qt/AccountTree.hpp"  // IWYU pragma: associated

#include <QObject>
#include <QString>
#include <QVariant>
#include <memory>
#include <utility>

#include "interface/ui/accounttree/AccountCurrency.hpp"
#include "interface/ui/accounttree/AccountTreeItem.hpp"
#include "internal/interface/ui/UI.hpp"
#include "internal/util/Size.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/core/Amount.hpp"
#include "util/Polarity.hpp"  // IWYU pragma: keep

namespace opentxs::factory
{
auto AccountTreeQtModel(ui::internal::AccountTree& parent) noexcept
    -> std::unique_ptr<ui::AccountTreeQt>
{
    using ReturnType = ui::AccountTreeQt;

    return std::make_unique<ReturnType>(parent);
}
}  // namespace opentxs::factory

namespace opentxs::ui
{
struct AccountTreeQt::Imp {
    internal::AccountTree& parent_;

    Imp(internal::AccountTree& parent)
        : parent_(parent)
    {
    }
};

AccountTreeQt::AccountTreeQt(internal::AccountTree& parent) noexcept
    : Model(parent.GetQt())
    , imp_(std::make_unique<Imp>(parent).release())
{
    if (nullptr != internal_) {
        internal_->SetColumnCount(nullptr, 1);
        internal_->SetRoleData({
            {AccountTreeQt::NameRole, "name"},
            {AccountTreeQt::NotaryIDRole, "notaryid"},
            {AccountTreeQt::NotaryNameRole, "notaryname"},
            {AccountTreeQt::UnitRole, "unit"},
            {AccountTreeQt::UnitNameRole, "unitname"},
            {AccountTreeQt::AccountIDRole, "account"},
            {AccountTreeQt::BalanceRole, "balance"},
            {AccountTreeQt::PolarityRole, "polarity"},
            {AccountTreeQt::AccountTypeRole, "accounttype"},
            {AccountTreeQt::ContractIdRole, "contractid"},
        });
    }
}

AccountTreeQt::~AccountTreeQt()
{
    if (nullptr != imp_) {
        delete imp_;
        imp_ = nullptr;
    }
}
}  // namespace opentxs::ui

namespace opentxs::ui::implementation
{
auto AccountCurrency::qt_data(const int column, const int role, QVariant& out)
    const noexcept -> void
{
    using Parent = AccountTreeQt;

    switch (role) {
        case Qt::DisplayRole: {
            switch (column) {
                case Parent::NameColumn: {
                    qt_data(column, Parent::NameRole, out);
                } break;
                default: {
                }
            }
        } break;
        case Parent::NameRole: {
            out = Name().c_str();
        } break;
        case Parent::UnitRole: {
            out = static_cast<int>(Currency());
        } break;
        default: {
        }
    }
}

auto AccountTreeItem::qt_data(const int column, const int role, QVariant& out)
    const noexcept -> void
{
    using Parent = AccountTreeQt;

    switch (role) {
        case Qt::DisplayRole: {
            switch (column) {
                case Parent::NameColumn: {
                    qt_data(column, Parent::NameRole, out);
                } break;
                default: {
                }
            }
        } break;
        case Parent::NameRole: {
            out = Name().c_str();
        } break;
        case Parent::NotaryIDRole: {
            out = NotaryID().c_str();
        } break;
        case Parent::NotaryNameRole: {
            out = NotaryName().c_str();
        } break;
        case Parent::UnitRole: {
            out = static_cast<int>(Unit());
        } break;
        case Parent::UnitNameRole: {
            out = DisplayUnit().c_str();
        } break;
        case Parent::AccountIDRole: {
            out = AccountID().c_str();
        } break;
        case Parent::BalanceRole: {
            out = DisplayBalance().c_str();
        } break;
        case Parent::PolarityRole: {
            out = polarity(Balance());
        } break;
        case Parent::AccountTypeRole: {
            out = static_cast<int>(Type());
        } break;
        case Parent::ContractIdRole: {
            out = ContractID().c_str();
        } break;
        case Parent::UnitDescriptionRole: {
            const auto text = print(Unit());
            out = QString::fromUtf8(text.data(), size_to_int(text.size()));
        } break;
        default: {
        }
    }
}
}  // namespace opentxs::ui::implementation
