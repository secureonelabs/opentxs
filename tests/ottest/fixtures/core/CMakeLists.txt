# Copyright (c) 2010-2023 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_subdirectory(contract)

target_sources(
  opentxs-testlib
  PRIVATE
    "Armored.cpp"
    "Armored.hpp"
    "Identifier.cpp"
    "Identifier.hpp"
    "Ledger.cpp"
    "Ledger.hpp"
    "PaymentCode.cpp"
    "PaymentCode.hpp"
    "StateMachine.cpp"
    "StateMachine.hpp"
)
