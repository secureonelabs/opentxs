# Copyright (c) 2010-2022 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

target_sources(opentxs-testlib PRIVATE "Helpers.hpp" "Helpers.cpp")

if(OT_ENABLE_RPC)
  target_sources(opentxs-testlib PRIVATE "Helpers-rpc.cpp")
  # target_sources(opentxs-testlib PRIVATE "Rpc.hpp" "Rpc.cpp")
  # target_sources(opentxs-testlib PRIVATE "RpcAsync.hpp" "RpcAsync.cpp")

  if(OT_BLOCKCHAIN_EXPORT)
    target_sources(
      opentxs-testlib PRIVATE "SendPayment_blockchain.cpp"
                              "SendPayment_blockchain.hpp"
    )
  endif()
else()
  target_sources(opentxs-testlib PRIVATE "Null.cpp")
endif()
