//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2022         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2022         David Sommerseth <davids@openvpn.net>
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
 * @file   logtag.hpp
 *
 * @brief  Declaration of the LogTag class
 */

#pragma once

#include <iostream>
#include <string>

/**
 *  This provides a more generic interface to generate and process
 *  the log tags and hashes used to separate log events from various
 *  attached log senders.
 */
struct LogTag
{
    /**
     *  LogTag constructor
     *
     * @param sender     std::string of the D-Bus unique bus name (1:xxxx)
     * @param interface  std::string of the D-Bus interface sending events
     *
     */
    LogTag(std::string sender, std::string interface, const bool default_encaps=true);

    LogTag();
    LogTag(const LogTag& cp);

    virtual ~LogTag();

    /**
     *  Return a std::string containing the tag to be used with log lines
     *
     *  The structure is: {tag:xxxxxxxxxxx}
     *  where xxxxxxxxxxx is a positive number if the 'encaps' is set to true.
     *
     *  Without the 'encaps' enabled, it will just return the positive number
     *  as a string.
     *
     * @param override   Bool flag to override the default encapsulating of the
     *                   tag hash.
     *
     * @return  Returns a std::string containing the tag this sender and
     *          interface will use
     */
     virtual const std::string str(const bool override) const;

     /**
      *  This is the same as the @str(const bool) variant, just that it will
      *  use the default_encaps setting given to the constructor.
      *
     * @return  Returns a std::string containing the tag this sender and
     *          interface will use
      */
     const std::string str() const;

    /**
     *  Write a formatted tag string via iostreams
     *
     * @param os  std::ostream where to write the data
     * @param ev  LogEvent to write to the stream
     *
     * @return  Returns the provided std::ostream together with the
     *          decoded LogEvent information
     */
    friend std::ostream& operator<<(std::ostream& os , const LogTag& ltag)
    {
        return os << ltag.str();
    }


    std::string tag{};    /**<  Contains the string used for the hash generation */
    size_t hash{};        /**<  Contains the hash value for this LogTag */
    bool encaps = true;   /**<  Encapsulate the hash value in "{tag:...}" */
};
