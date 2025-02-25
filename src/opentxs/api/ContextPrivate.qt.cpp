// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "opentxs/api/ContextPrivate.hpp"  // IWYU pragma: associated

#include <QObject>
#include <memory>

namespace opentxs::api
{
auto ContextPrivate::get_qt() const noexcept -> std::unique_ptr<QObject>&
{
    static auto qt = std::make_unique<QObject>();

    return qt;
}

auto ContextPrivate::shutdown_qt() noexcept -> void { get_qt().reset(); }

auto ContextPrivate::QtRootObject(QObject* parent) const noexcept -> QObject*
{
    auto& qt = get_qt();

    if (qt) {
        auto* effective = [&]() -> QObject* {
            if (nullptr != parent) {

                return parent;
            } else {

                return args_.QtRootObject();
            }
        }();

        if ((nullptr != effective) && (qt->thread() != effective->thread())) {
            qt->moveToThread(effective->thread());
        }
    }

    return qt.get();
}
}  // namespace opentxs::api
