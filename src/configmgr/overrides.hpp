//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2024-  Razvan Cojocaru <razvan.cojocaru@openvpn.com>
//

/**
 * @file   overrides.hpp
 *
 * @brief  Code needed to handle configuration overrides
 */

#pragma once

#include <optional>
#include <string>
#include <variant>


/**
 * Helper classes to store the list of overrides
 */
struct Override
{
    Override(const std::string &key, const std::string &value, const std::string &help, std::string (*argument_helper)() = nullptr)
        : key(key), value(value), help(help), argument_helper(argument_helper)
    {
    }


    Override(const std::string &key, bool value, const std::string &help, std::string (*argument_helper)() = nullptr)
        : key(key), value(value), help(help), argument_helper(argument_helper)
    {
    }


    std::string key;
    std::variant<std::string, bool> value;
    std::string help;
    std::string (*argument_helper)() = nullptr;
};


const Override configProfileOverrides[] = {
    // clang-format off
    {"server-override", std::string {},
     "Replace the remote, connecting to this server instead the server specified in the configuration"},

    {"port-override", std::string {},
     "Replace the remote port, connecting to this port instead of the configuration value"},

    {"proto-override", std::string {},
     "Overrides the protocol being used",
     [] {return std::string("tcp udp");}},

    {"ipv6", std::string {},
     "Sets the IPv6 policy of the client",
     [] { return std::string("yes no default");}},

    {"persist-tun", false,
     "The tun interface should persist during reconnect"},

    {"log-level", std::string {},
     "Override the configuration profile --verb setting",
     [] { return std::string("1 2 3 4 5 6");}},

    {"dns-fallback-google", false,
     "Uses Google DNS servers (8.8.8.8/8.8.4.4) if no DNS server are provided"},

    {"dns-setup-disabled", false,
     "Do not change the DNS settings on the system"},

    {"dns-scope", std::string {},
     "Defines which domain scope can be queried via VPN provided DNS servers",
      [] {return std::string("global tunnel");}},

    {"dns-sync-lookup", false,
     "Use synchronous DNS Lookups"},

    {"auth-fail-retry", false,
     "Should failed authentication be considered a temporary error"},

    {"allow-compression", std::string {},
     "Set compression mode",
     [] {return std::string("no asym yes");}},

    {"enable-legacy-algorithms", false,
     "Enable legacy non-AEAD cipher algorithms for the data channel",
     [] {return std::string("true false");}},

    {"tls-version-min", std::string {},
     "Sets the minimal TLS version for the control channel",
     [] {return std::string("tls_1_0 tls_1_1 tls_1_2 tls_1_3");}},

    {"tls-cert-profile", std::string {},
     "Sets the control channel tls profile",
     [] {return std::string("legacy preferred suiteb");}},

    {"proxy-host", std::string {},
     "HTTP Proxy to connect via, overrides configuration file http-proxy"},

    {"proxy-port", std::string {},
     "HTTP Proxy port to connect on"},

    {"proxy-username", std::string {},
     "HTTP Proxy username to authenticate as"},

    {"proxy-password", std::string {},
     "HTTP Proxy password to use for authentication"},

    {"proxy-auth-cleartext", false,
     "Allows clear text HTTP authentication"},

    {"enterprise-profile", std::string {},
     "Enterprise profile for client side device posture checks"},

    {"automatic-restart", std::string {},
     "Restart policy for backend client processes",
     [] {return std::string("no on-failure");}}
    // clang-format on
};


std::optional<Override> GetConfigOverride(const std::string &key,
                                          bool ignoreCase = false);
