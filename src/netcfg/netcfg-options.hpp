//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         Arne Schwabe <arne@openvpn.net>
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
 * @file   netcfg-options.hpp
 *
 * @brief  The implementation of options that are use by netcfg
 */

#pragma once

#include "common/cmdargparser.hpp"


enum class RedirectMethod : std::uint8_t
{
    NONE = 0,    //<  Do not add any additional routes
    HOST_ROUTE,  //<  Add direct route to VPN server
    BINDTODEV    //<  Bind the UDP/TCP socket to the default gw interface
};


/**
 * struct to hold the setting for the Netcfg service
 */
struct NetCfgOptions {
    /** Decides wether use the tun-builder redirect functionality */
    RedirectMethod redirect_method = RedirectMethod::HOST_ROUTE;

    /** the SO_MARK to use if > 0 */
    int so_mark = -1;


    NetCfgOptions(ParsedArgs& args)
    {
        if (args.Present("redirect-method"))
        {
            std::string method = args.GetValue("redirect-method", 0);
            if ("none" == method)
            {
                redirect_method = RedirectMethod::NONE;
            }
            else if ("host-route" == method)
            {
                redirect_method = RedirectMethod::HOST_ROUTE;
            }
            else if ("bind-device" == method)
            {
                redirect_method = RedirectMethod::BINDTODEV;
            }
            else
            {
                throw CommandArgBaseException("Invalid argument to --redirect-method: "
                                              + method);
            }
        }

        if (args.Present("set-somark"))
        {
            so_mark = std::atoi(args.GetValue("set-somark", 0).c_str());
        }
    }


    NetCfgOptions(const NetCfgOptions& origin) noexcept
    {
        redirect_method = origin.redirect_method;
        so_mark = origin.so_mark;
    }


    /**
     *  Generate a decoded string of the current options
     *
     * @return  Returns a const std::string of the decoded options
     */
    const std::string str()
    {
        return decode_options(*this);
    }


    /**
     *  Makes it possible to write NetCfgOptions in a readable format
     *  via iostreams.
     *
     * @param os  std::ostream where to write the data
     * @param ev  NetCfgOptions to write to the stream
     *
     * @return  Returns the provided std::ostream together with the
     *          decoded NetCfgOptions information
     */
    friend std::ostream& operator<<(std::ostream& os , const NetCfgOptions& o)
    {
        return os << decode_options(o);
    }


private:
    static const std::string decode_options(const NetCfgOptions& o)
    {
        std::stringstream s;

        s << "Redirect method: ";
        switch (o.redirect_method)
        {
        case RedirectMethod::NONE:
            s << "None";
            break;
        case RedirectMethod::BINDTODEV:
            s << "bind-device";
            break;
        case RedirectMethod::HOST_ROUTE:
            s << "host-route";
            break;
        }

        if (o.so_mark >= 0)
        {
            s << ", so-mark: " << std::to_string(o.so_mark);
        }
        return s.str();
    }
};
