//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2022  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   timestamp.cpp
 *
 * @brief  Simple functions for retriveving time/date related information
 */

#include <sstream>
#include <iomanip>


/**
 *  Get a timestamp of the current date and time.  The format is
 *  the ISO standard without time zone - YYYY-MM-DD HH:MM:SS
 *
 * @return  Returns a string with the date and time
 */
std::string GetTimestamp()
{
    time_t now = time(0);
    tm *ltm = localtime(&now);

    std::stringstream ret;
    ret << 1900 + ltm->tm_year
        << "-" << std::setw(2) << std::setfill('0') << 1 + ltm->tm_mon
        << "-" << std::setw(2) << std::setfill('0') << ltm->tm_mday
        << " " << std::setw(2) << std::setfill('0') << ltm->tm_hour
        << ":" << std::setw(2) << std::setfill('0') << ltm->tm_min
        << ":" << std::setw(2) << std::setfill('0') << ltm->tm_sec
        << " ";
    return ret.str();
}
