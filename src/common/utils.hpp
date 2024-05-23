//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   utils.hpp
 *
 * @brief  Misc utility functions
 */

#pragma once

#include <ctime>
#include <string>

#include "build-version.h"

#ifndef CONFIGURE_GIT_REVISION
constexpr char package_version[] = PACKAGE_GUIVERSION;
#else
constexpr char package_version[] = "git:" CONFIGURE_GIT_REVISION CONFIGURE_GIT_FLAGS;
#endif


void drop_root();
std::string get_version(std::string component);
const std::string get_guiversion();
int stop_handler(void *loop);
void set_console_echo(bool echo);

static inline std::string simple_basename(const std::string filename)
{
    return filename.substr(filename.rfind('/') + 1);
}


/**
 *  Converts a time_t (epoch) value to a human readable
 *  date/time string, based on the local time zone.
 *
 * @param epoch         time_t value to convert
 * @return std::string containing the human readable representation
 */
std::string get_local_tstamp(const std::time_t epoch);


/**
 *  Checks if the currently available console/terminal is
 *  capable of doing colours.
 *
 * @return bool, true if it is expected the terminal output can handle
 *               colours; otherwise false
 */
bool is_colour_terminal();
