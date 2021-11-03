//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2021         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2021         David Sommerseth <davids@openvpn.net>
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
 * @file   machineid.hpp
 *
 * @brief  Generate a static unique machine ID
 */

#pragma once

#include <iostream>
#include <exception>

#define OPENVPN3_MACHINEID std::string(std::string(OPENVPN3_STATEDIR) + "/machine-id")

class MachineIDException : std::exception
{
public:
    MachineIDException(const std::string& msg) noexcept;
    std::string GetError() const noexcept;
    const char *what();
private:
    std::string error;
};


/**
 *  This is used to retrieve a static and unique identify of the
 *  host the application is running on.  It will by default attempt to us
 *  /etc/machine-id as the source of information.  If not, it will generate
 *  it's own UUID based ID and preserve it in the OpenVPN 3 state directory
 *  (typically /var/lib/openvpn3).
 *
 *  If it fails preserve the generated machine-id value, the result will be
 *  random for each time this class is initiated.  To check whether the result
 *  will be static or random, call the success() method - if that does not throw
 *  an exception, the available machine-id will be static.
 */
class MachineID
{
public:
    enum class SourceType {
        NONE,        ///< No source used - no reliable machine-id available if any at all
        SYSTEM,      ///< machine-id derived from /etc/machine-id
        SYSTEMD_API, ///< machine-id derived from systemd API
        LOCAL,       ///< Static machine-id created, using OPENVPN3_MACHINEID
        RANDOM       ///< Storing generated machine-id failed, unreliable random value used
    };


    MachineID(const std::string& local_machineid = OPENVPN3_MACHINEID,
              bool enforce_local=false);
    virtual ~MachineID() = default;

    /**
     *  Checks if a static machine-id was retrieved successfully.  If not,
     *  the machine-id will be random and this method will throw a
     *  MachineIDException with the reason why it is not static.
     *
     *  @throws MachineIDException
     */
    void success() const;

    /**
     *  Retrieve the source type for the generation of the machine-id
     *
     * @return  Returns MachineID::SourceType used
     */
    SourceType GetSource() const noexcept;

    std::string get() const noexcept;

    friend std::ostream& operator<<(std::ostream& os,
                                    const MachineID& machid)
    {
        return os << machid.machine_id;
    }

private:
    SourceType source{SourceType::NONE};
    std::string machine_id{};
    std::string errormsg{};

    std::string generate_machine_id(const std::string& fname) noexcept;
};
