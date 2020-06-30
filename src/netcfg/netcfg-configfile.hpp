//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2020         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020         David Sommerseth <davids@openvpn.net>
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
    NetCfgConfigFile() : Configuration::File()
    {
    }


protected:
    Configuration::OptionMap ConfigureMapping()
    {
        return {
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
            };
    }
};
