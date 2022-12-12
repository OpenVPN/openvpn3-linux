//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2020 - 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020 - 2022  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   netcfg-configfile.hpp
 *
 * @brief  Configuration of the JSON configuration file to be used by
 *         openvpn3-service-netcfg.
 */

#pragma once

#include <memory>
#include "common/configfileparser.hpp"

using namespace Configuration;


class NetCfgConfigFile : public virtual Configuration::File
{
  public:
    typedef std::shared_ptr<Configuration::File> Ptr;
    NetCfgConfigFile()
        : Configuration::File()
    {
    }

  protected:
    Configuration::OptionMap ConfigureMapping()
    {
        return {
            // clang-format off
            OptionMapEntry{"log-level", "log_level",
                           "Log level", OptionValueType::Int},
            OptionMapEntry{"log-file", "log_file",
                           "Log destination", OptionValueType::String},
            OptionMapEntry{"idle-exit", "idle_exit",
                           "Idle-Exit timer", OptionValueType::Int},
            OptionMapEntry{"resolv-conf", "resolv_conf_file", "DNS-resolver",
                           "resolv-conf file", OptionValueType::String},
            OptionMapEntry{"systemd-resolved", "systemd_resolved", "DNS-resolver",
                           "Systemd-resolved in use", OptionValueType::Present},
            OptionMapEntry{"redirect-method", "redirect_method",
                           "Server route redirection mode", OptionValueType::String},
            OptionMapEntry{"set-somark", "set_somark",
                           "Netfilter SO_MARK", OptionValueType::String}
            // clang-format on
        };
    }
};
