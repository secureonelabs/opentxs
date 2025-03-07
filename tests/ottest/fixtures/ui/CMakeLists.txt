# Copyright (c) 2010-2023 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

target_sources(
  opentxs-testlib
  PRIVATE
    "AccountActivity.cpp"
    "AccountActivity.hpp"
    "AccountList.cpp"
    "AccountList.hpp"
    "AccountTree.cpp"
    "AccountTree.hpp"
    "AmountValidator.hpp"
    "BlockchainAccountStatus.cpp"
    "BlockchainAccountStatus.hpp"
    "BlockchainSelection.cpp"
    "BlockchainSelection.hpp"
    "BlockchainSelector.cpp"
    "BlockchainSelector.hpp"
    "ContactActivity.cpp"
    "ContactActivity.hpp"
    "ContactList.cpp"
    "ContactList.hpp"
    "Items.cpp"
    "Items.hpp"
    "NymList.cpp"
    "NymList.hpp"
    "SeedTree.cpp"
    "SeedTree.hpp"
)

if(OT_QT_EXPORT)
  target_sources(opentxs-testlib PRIVATE "Qt.cpp")
else()
  target_sources(opentxs-testlib PRIVATE "no-Qt.cpp")
endif()
