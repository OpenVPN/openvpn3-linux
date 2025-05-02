//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2025-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2025-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   common/string-utils.hpp
 *
 * @brief  Collection of various minor helper functions for strings
 */

#pragma once

#include <string>


/**
 *  Filters out all characters below 0x20 (ASCII control characters).
 *  Newline characters are removed if filter_nl is set to be true.
 *
 *  @param input      std::string of the input string to filter
 *  @param filter_nl  bool flag to enable filtering of newline chars.
 *                    If true, newline chars will be removed.
 *  @return std::string of the santised input string
 */
std::string filter_ctrl_chars(const std::string input, bool filter_nl);