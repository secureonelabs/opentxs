// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "opentxs/interface/qt/AmountValidator.hpp"  // IWYU pragma: associated

#include "interface/qt/AmountValidator.hpp"
#include "interface/ui/accountactivity/AccountActivity.hpp"  // IWYU pragma: keep
#include "opentxs/Types.hpp"
#include "opentxs/UnitType.hpp"  // IWYU pragma: keep

namespace opentxs::ui
{
auto AmountValidator::Imp::unittype() const noexcept -> UnitType
{
    if (false == unittype_.has_value()) { unittype_ = parent_.Unit(); }

    return unittype_.has_value() ? unittype_.value() : UnitType::Error;
}

AmountValidator::AmountValidator(
    implementation::AccountActivity& parent) noexcept
    : imp_(std::make_unique<Imp>(parent))
{
    assert_false(nullptr == imp_);
}

auto AmountValidator::fixup(QString& input) const -> void
{
    imp_->fixup(input);
}

auto AmountValidator::getMaxDecimals() const -> int
{
    return imp_->getMaxDecimals();
}

auto AmountValidator::getMinDecimals() const -> int
{
    return imp_->getMinDecimals();
}

auto AmountValidator::getScale() const -> int { return imp_->getScale(); }

auto AmountValidator::revise(QString& input, int previous) const -> QString
{
    return imp_->revise(input, previous);
}

auto AmountValidator::setMaxDecimals(int value) -> void
{
    if (imp_->setMaxDecimals(value)) {
        Q_EMIT scaleChanged(imp_->scale_.load());
    }
}

auto AmountValidator::setMinDecimals(int value) -> void
{
    if (imp_->setMinDecimals(value)) {
        Q_EMIT scaleChanged(imp_->scale_.load());
    }
}

auto AmountValidator::setScale(int value) -> void
{
    auto old = int{};

    if (imp_->setScale(value, old)) { Q_EMIT scaleChanged(old); }
}

auto AmountValidator::validate(QString& input, int& pos) const -> State
{
    return imp_->validate(input, pos);
}

AmountValidator::~AmountValidator() = default;
}  // namespace opentxs::ui
