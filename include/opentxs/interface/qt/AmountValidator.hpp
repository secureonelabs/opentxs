// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <QMetaObject>
#include <QString>
#include <QValidator>
#include <memory>

#include "opentxs/Export.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs
{
namespace ui
{
namespace implementation
{
class AccountActivity;
}  // namespace implementation
}  // namespace ui
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::ui
{
class OPENTXS_EXPORT AmountValidator final : public QValidator
{
    Q_OBJECT
    Q_PROPERTY(int maxDecimals READ getMaxDecimals WRITE setMaxDecimals)
    Q_PROPERTY(int minDecimals READ getMinDecimals WRITE setMinDecimals)
    Q_PROPERTY(int scale READ getScale WRITE setScale)

Q_SIGNALS:
    void scaleChanged(int previousValue);

public Q_SLOTS:
    void setMaxDecimals(int);
    void setMinDecimals(int);
    void setScale(int);

public:
    // NOLINTBEGIN(modernize-use-trailing-return-type)
    Q_INVOKABLE QString revise(QString& input, int previousValue) const;
    // NOLINTEND(modernize-use-trailing-return-type)

public:
    auto getMaxDecimals() const -> int;
    auto getMinDecimals() const -> int;
    auto getScale() const -> int;
    auto fixup(QString& input) const -> void final;
    auto validate(QString& input, int& pos) const -> State final;

    AmountValidator(implementation::AccountActivity& parent) noexcept;
    AmountValidator() = delete;
    AmountValidator(const AmountValidator&) = delete;
    AmountValidator(AmountValidator&&) = delete;
    auto operator=(const AmountValidator&) -> AmountValidator& = delete;
    auto operator=(AmountValidator&&) -> AmountValidator& = delete;

    ~AmountValidator() final;

private:
    struct Imp;

    std::unique_ptr<Imp> imp_;
};
}  // namespace opentxs::ui
