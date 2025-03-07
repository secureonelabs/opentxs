// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "ottest/fixtures/blockchain/SyncListener.hpp"
// IWYU pragma: no_include "ottest/fixtures/blockchain/TXOState.hpp"

#pragma once

#include <opentxs/opentxs.hpp>

#include "ottest/fixtures/blockchain/regtest/Base.hpp"

namespace ottest
{
class OPENTXS_EXPORT Regtest_fixture_normal : public Regtest_fixture_base
{
protected:
    Regtest_fixture_normal(const ot::api::Context& ot, const int clientCount);
    Regtest_fixture_normal(
        const ot::api::Context& ot,
        const int clientCount,
        ot::Options clientArgs);
    Regtest_fixture_normal(
        const ot::api::Context& ot,
        const int clientCount,
        ot::Options minerArgs,
        ot::Options clientArgs);
};
}  // namespace ottest
