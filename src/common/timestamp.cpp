//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         David Sommerseth <davids@openvpn.net>
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
