//    OpenVPN -- An application to securely tunnel IP networks
//               over a single port, with support for SSL/TLS-based
//               session authentication and key exchange,
//               packet encryption, packet authentication, and
//               packet compression.
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2012 - 2022  OpenVPN Inc <sales@openvpn.net>
//

// OpenVPN 3 test client

#define USE_TUN_BUILDER
#define USE_NETCFG
#define OPENVPN3_CORE_CLI_TEST

#ifdef HAVE_CONFIG_VERSION_H
#include "config-version.h"
#endif
#include "config.h"
#include <openvpn/log/logsimple.hpp>
#include <test/ovpncli/cli.cpp>
