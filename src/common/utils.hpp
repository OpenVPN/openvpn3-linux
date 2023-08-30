//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   utils.hpp
 *
 * @brief  Misc utility functions
 */

#pragma once

#include <string>

const char *package_version();

void drop_root();
std::string get_version(std::string component);
const std::string get_guiversion();
int stop_handler(void *loop);
void set_console_echo(bool echo);

static inline std::string simple_basename(const std::string filename)
{
    return filename.substr(filename.rfind('/') + 1);
}
