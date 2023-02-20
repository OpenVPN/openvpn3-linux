//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2022 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2022 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   service-configfile.hpp
 *
 * @brief  Definition of the elements used in the openvpn3-service-logger
 *         configuration file
 */

#pragma once

#include <memory>
#include <string>
#include "common/configfileparser.hpp"

using namespace Configuration;


class LogServiceConfigFile : public virtual Configuration::File
{
  public:
    typedef std::shared_ptr<Configuration::File> Ptr;

    LogServiceConfigFile()
        : Configuration::File("")
    {
    }

    LogServiceConfigFile(const std::string &statedir)
        : Configuration::File(statedir + "/" + "log-service.json")
    {
    }


  protected:
    Configuration::OptionMap ConfigureMapping()
    {
        return {
            // clang-format off
            OptionMapEntry{"journald", "journald",
                           "log_method_group",
                           "Use systemd-journald",
                           OptionValueType::Present},
            OptionMapEntry{"syslog", "syslog",
                           "log_method_group",
                           "Use syslog",
                           OptionValueType::Present},
            OptionMapEntry{"syslog-facility", "syslog_facility",
                           "Syslog facility",
                           OptionValueType::String},
            OptionMapEntry{"log-file", "log_file",
                           "log_method_group",
                           "Log file",
                           OptionValueType::String},
            OptionMapEntry{"colour", "log_file_colour",
                           "Colour log lines in log file",
                           OptionValueType::Present},
            OptionMapEntry{"log-level", "log_level",
                           "Log level",
                           OptionValueType::Int},
            OptionMapEntry{"service-log-dbus-details", "log_dbus_details",
                           "Add D-Bus details in log file/syslog",
                           OptionValueType::Present},
            OptionMapEntry{"timestamp", "timestamp",
                           "Add timestamp log file",
                           OptionValueType::Present},
            OptionMapEntry{"no-logtag-prefix", "no_logtag_prefix",
                           "Disable LogTag prefixes (systemd-journald)",
                           OptionValueType::Present},
            OptionMapEntry{"idle-exit", "idle_exit",
                           "Idle exit timer (minutes)", OptionValueType::Int},
            // clang-format on
        };
    }
};
