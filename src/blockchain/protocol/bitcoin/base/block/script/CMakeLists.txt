# Copyright (c) 2010-2022 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

libopentxs_add_public_header(
  "blockchain/protocol/bitcoin/base/block" "Script.hpp"
)

target_sources(
  opentxs-common
  PRIVATE
    "${opentxs_SOURCE_DIR}/src/internal/blockchain/protocol/bitcoin/base/block/Script.hpp"
    "Script.cpp"
    "ScriptPrivate.cpp"
    "ScriptPrivate.hpp"
    "Internal.cpp"
)

if(OT_BLOCKCHAIN_EXPORT)
  target_sources(
    opentxs-common
    PRIVATE
      "Factory.cpp"
      "Imp.cpp"
      "Imp.hpp"
  )
else()
  target_sources(opentxs-common PRIVATE "Null.cpp")
endif()
