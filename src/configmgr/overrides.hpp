//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2019  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2019  Arne Schwabe <arne@openvpn.net>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Affero General Public License as
//  published by the Free Software Foundation, version 3 of the
//  License.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Affero General Public License for more details.
//
//  You should have received a copy of the GNU Affero General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

/**
 * @file   overrides.hpp
 *
 * @brief  Code needed to handle configuration overrides
 */

#pragma once

enum class OverrideType
{
    string,
    boolean,
    invalid
};

/**
 * Helper classes to store the list of overrides
 */
struct ValidOverride {
    ValidOverride(std::string key, OverrideType type, std::string help)
        : key(key), type(type), help(help)
    {
    }

    ValidOverride(std::string key, OverrideType type,
                  std::string help, std::string (*argument_helper)())
        : key(key), type(type), help(help), argument_helper(argument_helper)
    {
    }


    inline bool valid() const
    {
        return type != OverrideType::invalid;
    }


    std::string key;
    OverrideType type;
    std::string help;
    std::string (*argument_helper)()=nullptr;
};


struct OverrideValue {
    OverrideValue(const ValidOverride& override, bool value)
        : override(override), boolValue(value)
    {
    }


    OverrideValue(const ValidOverride& override, std::string value)
        : override(override), strValue(value)
    {
    }


    ValidOverride override;
    bool boolValue;
    std::string strValue;
};


const ValidOverride configProfileOverrides[] = {
    {"server-override", OverrideType::string,
     "Replace the remote, connecting to this server instead the server specified in the configuration"},

    {"port-override", OverrideType::string,
     "Replace the remote port, connecting to this port instead of the configuration value"},

    {"proto-override", OverrideType::string,
     "Overrides the protocol being used",
     [] {return std::string("tcp udp");}},

    {"ipv6", OverrideType::string,
     "Sets the IPv6 policy of the client",
     [] { return std::string("yes no default");}},

    {"persist-tun", OverrideType::boolean,
     "The tun interface should persist during reconnect"},

    {"log-level", OverrideType::string,
     "Override the configuration profile --verb setting",
     [] { return std::string("1 2 3 4 5 6");}},

    {"dns-fallback-google", OverrideType::boolean,
     "Uses Google DNS servers (8.8.8.8/8.8.4.4) if no DNS server are provided"},

    {"dns-setup-disabled", OverrideType::boolean,
     "Do not change the DNS settings on the system"},

    {"dns-scope", OverrideType::string,
     "Defines which domain scope can be queried via VPN provided DNS servers",
      [] {return std::string("global tunnel");}},

    {"dns-sync-lookup", OverrideType::boolean,
     "Use synchronous DNS Lookups"},

    {"auth-fail-retry", OverrideType::boolean,
     "Should failed authentication be considered a temporary error"},

    {"allow-compression", OverrideType::string,
     "Set compression mode",
     [] {return std::string("no asym yes");}},

    {"enable-legacy-algorithms", OverrideType::boolean,
     "Enable legacy non-AEAD cipher algorithms for the data channel",
     [] {return std::string("true false");}},

    {"tls-version-min", OverrideType::string,
     "Sets the minimal TLS version for the control channel",
     [] {return std::string("tls_1_0 tls_1_1 tls_1_2 tls_1_3");}},

    {"tls-cert-profile", OverrideType::string,
     "Sets the control channel tls profile",
     [] {return std::string("legacy preferred suiteb");}},

    {"proxy-host", OverrideType::string,
     "HTTP Proxy to connect via, overrides configuration file http-proxy"},

    {"proxy-port", OverrideType::string,
     "HTTP Proxy port to connect on"},

    {"proxy-username", OverrideType::string,
     "HTTP Proxy username to authenticate as"},

    {"proxy-password", OverrideType::string,
     "HTTP Proxy password to use for authentication"},

    {"proxy-auth-cleartext", OverrideType::boolean,
     "Allows clear text HTTP authentication"}
};


const ValidOverride invalidOverride(std::string("invalid"),
                                    OverrideType::invalid, "Invalid override");


const ValidOverride & GetConfigOverride(const std::string& key,
                                        bool ignoreCase = false);
