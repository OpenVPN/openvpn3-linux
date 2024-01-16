//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2021 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2021 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   machineid.hpp
 *
 * @brief  Generate a static unique machine ID
 */

#pragma once

#include <iostream>
#include <exception>
#include <string_view>


class MachineIDException : public std::exception
{
  public:
    MachineIDException(const std::string &msg) noexcept;
    std::string GetError() const noexcept;
    const char *what() const noexcept override;

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
    enum class SourceType
    {
        NONE,        ///< No source used - no reliable machine-id available if any at all
        SYSTEM,      ///< machine-id derived from /etc/machine-id
        SYSTEMD_API, ///< machine-id derived from systemd API
        LOCAL,       ///< Static machine-id created, using OPENVPN3_MACHINEID
        RANDOM       ///< Storing generated machine-id failed, unreliable random value used
    };

    MachineID(const std::string &local_machineid = "",
              bool enforce_local = false);
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


    friend std::ostream &operator<<(std::ostream &os,
                                    const MachineID &machid)
    {
        return os << machid.machine_id;
    }


  private:
    SourceType source{SourceType::NONE};
    std::string machine_id{};
    std::string errormsg{};

    std::string generate_machine_id(const std::string &fname) noexcept;
};
