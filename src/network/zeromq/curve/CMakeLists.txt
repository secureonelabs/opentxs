# Copyright (c) 2010-2022 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

target_sources(
  opentxs-common
  PRIVATE
    "${opentxs_SOURCE_DIR}/src/internal/network/zeromq/curve/Client.hpp"
    "${opentxs_SOURCE_DIR}/src/internal/network/zeromq/curve/Server.hpp"
    "Client.cpp"
    "Client.hpp"
    "Server.cpp"
    "Server.hpp"
)
libopentxs_link_internal("${OT_ZMQ_TARGET}")
libopentxs_link_external("${OT_ZMQ_TARGET}")
