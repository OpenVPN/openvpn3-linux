//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2025-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2025-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   common/string-utils.cpp
 *
 * @brief  Collection of various minor helper functions for strings
 */


#include <algorithm>
#include <string>


std::string filter_ctrl_chars(const std::string input, bool filter_nl)
{
    std::string output(input);

    // Remove trailing new lines
    output.erase(output.find_last_not_of("\n") + 1);

    // Remove all control characters (< 0x20) - except
    // of preserving only newlines (\n) if requested.
    output.erase(
        std::remove_if(output.begin(),
                       output.end(),
                       [filter_nl](char c)
                       {
                           // We allow characters being 0x20 or higher
                           // unless filter_nl is true, then we also allow
                           // only \n.
                           return ((c < 0x20 && c != '\n')
                                   || (filter_nl && c == '\n'));
                       }),
        output.end());
    return output;
}