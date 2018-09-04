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
 * @file   netcfg-stateevent-selftest.cpp
 *
 * @brief  Unit test for struct NetCfgStateEvent
 */

#include <iostream>
#include <string>
#include <sstream>

#include "dbus/core.hpp"
#include "netcfg/netcfg-stateevent.hpp"


bool test_empty(const NetCfgStateEvent& ev, const bool expect)
{
    bool ret = false;


    bool r = ev.empty();
    std::cout << "      test_empty():  ev.empty() = " << r << " ... ";
    if (expect != r)
    {
        std::cerr << "** ERROR **  Object not empty [empty()]: " << std::endl;
        ret = true;
    }
    else
    {
        std::cout << "PASSED" << std::endl;
    }


    r = (NetCfgStateType::UNSET == ev.type
         && ev.device.empty()
         && ev.details.empty());
    std::cout << "      test_empty():  Element check:"
              << " (" << std::to_string((unsigned) ev.type)
              << ", '" << ev.device
              << "', '" << ev.details << "') = " << r << " ... ";
    if (expect != r)
    {
        std::cerr << "** ERROR **  Object not empty [direct member check]"
                  << std::endl;
        ret = false;
    }
    else
    {
        std::cout << "PASSED" << std::endl;
    }
    return ret;
};


int test_init()
{
    int ret = 0;

    std::cout << "-- Testing just initialized object - empty" << std::endl;
    NetCfgStateEvent empty;

    if (test_empty(empty, true))
    {
        ++ret;
    }

    std::cout << "-- Testing just initialized object - init with values (1)" << std::endl;
    NetCfgStateEvent populated1(NetCfgStateType::DEVICE_ADDED, "test-dev", "Some detail");
    if (test_empty(populated1, false))  // This should fail
    {
        ++ret;
    }

    std::cout << "-- Testing just object with values being cleared" << std::endl;
    populated1.reset();
    if (test_empty(populated1, true))
    {
        ++ret;
    }

    return ret;
}


int test_stream()
{
    int ret = 0;

    std::cout << "-- Testing string stream: NetCfgState(NetCfgStateType::IPv6ADDR_ADDED, "
              << "'testdev', '2001:db8:a050::1/64') ... ";
    NetCfgStateEvent state(NetCfgStateType::IPv6ADDR_ADDED, "testdev", "2001:db8:a050::1/64");
    std::stringstream chk;
    chk << state;
    std::string expect("Device testdev - IPv6 Address Added: 2001:db8:a050::1/64");
    if (chk.str() != expect)
    {
        std::cout << "FAILED: {" << state << "}" << std::endl;
        ++ret;
    }
    else
    {
        std::cout << "PASSED" << std::endl;
    }

    return ret;
}


int test_gvariant()
{
    int ret = 0;

    std::cout << "-- Testing .GetGVariant() ... ";
    NetCfgStateEvent g_state(NetCfgStateType::IPv6ROUTE_ADDED, "tun22",
                           "2001:db8:bb50::/64 via 2001:db8:a050::1/64");
    GVariant *chk = g_state.GetGVariant();

    guint type = 0;
    gchar *dev_s = nullptr;
    gchar *det_s = nullptr;
    g_variant_get(chk, "(uss)", &type, &dev_s, &det_s);

    if ((guint) g_state.type != type
        || 0 != (g_state.device.compare(dev_s))
        || 0 != (g_state.details.compare(det_s))
        )
    {
        std::cout << "FAILED" << std::endl;
        std::cout << "     Input: " << g_state << std::endl;
        std::cout << "    Output: "
                  << "type=" << std::to_string(type) << ", "
                  << "device='" << dev_s << "', "
                  << "details='" << det_s<< "'" << std::endl;
        ++ret;
    }
    else
    {
        std::cout << "PASSED" << std::endl;
    }
    g_free(dev_s);
    g_free(det_s);
    g_variant_unref(chk);

    return ret;
}


int main(int argc, char **argv)
{
    bool failed = false;
    int r = 0;

    std::cout << "** NetCfgStateEvent Unit Test **" << std::endl;
    if ((r = test_init()) > 0)
    {
        std::cout << "** test_init() failed, result: " << r << std::endl;
        failed = true;
    }

    if ((r = test_stream()) > 0)
    {
        std::cout << "** test_stream() failed" << std::endl;
        failed = true;
    }

    if ((r = test_gvariant()) > 0)
    {
        std::cout << "** test_gvariant() failed" << std::endl;
        failed = true;
    }


    std::cout << std::endl
              << ">> OVERAL TEST RESULT: " << (failed ? "FAILED" : "PASSED")
              << std::endl << std::endl;
    return (failed ? 2: 0);
}
