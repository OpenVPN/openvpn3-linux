//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2019  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2019  David Sommerseth <davids@openvpn.net>
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
 * @file   netcfg-changeeevent-selftest.cpp
 *
 * @brief  Unit test for struct NetCfgChangeEvent
 */

#include <iostream>
#include <string>
#include <sstream>

#include "dbus/core.hpp"
#include "netcfg/netcfg-changeevent.hpp"


bool test_empty(const NetCfgChangeEvent& ev, const bool expect)
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


    r = (NetCfgChangeType::UNSET == ev.type
         && ev.device.empty()
         && ev.details.empty());
    std::cout << "      test_empty():  Element check:"
              << " (" << std::to_string((unsigned) ev.type)
              << ", '" << ev.device
              << "', details.size=" << ev.details.size() << ") = " << r << " ... ";
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
    NetCfgChangeEvent empty;

    if (test_empty(empty, true))
    {
        ++ret;
    }

    std::cout << "-- Testing just initialized object - init with values (1)" << std::endl;
    NetCfgChangeEvent populated1(NetCfgChangeType::DEVICE_ADDED, "test-dev",
                                 {{"some_key", "Some detail"}});
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

    std::cout << "-- Testing string stream: NetCfgChangeEvent(NetCfgChangeType::IPADDR_ADDED, "
              << "'testdev', '2001:db8:a050::1/64') ... ";
    NetCfgChangeEvent state(NetCfgChangeType::IPADDR_ADDED, "testdev",
                            {{"ip_address", "2001:db8:a050::1"},
                             {"prefix", "64"}});
    std::stringstream chk;
    chk << state;
    std::string expect("Device testdev - IP Address Added: ip_address='2001:db8:a050::1', prefix='64'");
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

    std::cout << "-- Testing .GetGVariant() ... " << std::endl;
    NetCfgChangeEvent g_state(NetCfgChangeType::ROUTE_ADDED, "tun22",
                              {{"ip_address", "2001:db8:a050::1"},
                               {"prefix", "64"}});
    GVariant *chk = g_state.GetGVariant();

    std::cout << "      g_variant_print() check: ";
    gchar *dmp = g_variant_print(chk, true);
    std::string dump_check(dmp);
    g_free(dmp);
    if (dump_check != "(uint32 16, 'tun22', {'ip_address': '2001:db8:a050::1', 'prefix': '64'})")
    {
        std::cout << "FAILED: " << dump_check;
    }
    else
    {
        std::cout << "PASSED";
    }
    std::cout << std::endl;


    std::cout << "      manual parsing: ";
    guint type = 0;
    gchar *dev_s = nullptr;
    GVariantIter *det_g = nullptr;
    g_variant_get(chk, "(usa{ss})", &type, &dev_s, &det_g);
    g_variant_unref(chk);

    NetCfgChangeDetails det_s;
    GVariant *kv = nullptr;
    while ((kv = g_variant_iter_next_value(det_g)))
    {
        gchar *key = nullptr;
        gchar *value = nullptr;
        g_variant_get(kv, "{ss}", &key, &value);

        det_s[key] = std::string(value);
        g_free(key);
        g_free(value);
    }
    g_variant_iter_free(det_g);

    if ((guint) g_state.type != type
        || 0 != (g_state.device.compare(dev_s))
        || g_state.details != det_s
        )
    {
        std::cout << "FAILED" << std::endl;
        std::cout << "     Input: " << g_state << std::endl;
        std::cout << "    Output: "
                  << "type=" << std::to_string(type) << ", "
                  << "device='" << dev_s << "', "
                  << "details={";

        bool f = true;
        for (const auto& kv : det_s)
        {
            std::cout << (f ? "" : ", ")
                      << kv.first << "='" << kv.second << "'";
            f = false;
        }
        std::cout << std::endl;

        ++ret;
    }
    else
    {
        std::cout << "PASSED" << std::endl;
    }
    g_free(dev_s);

    std::cout << "-- Testing parsing GVariant data (valid data)... ";
    NetCfgChangeEvent parsed(g_state);
    if (g_state != parsed)
    {
        std::cout << "FAILED" << std::endl;
        std::cout << "     Input: " << g_state << std::endl;
        std::cout << "    Output: " << parsed << std::endl;
        ++ret;
    }
    else
    {
        std::cout << "PASSED" << std::endl;
    }


    std::cout << "-- Testing parsing GVariant data (invalid data)... ";
    GVariant *invalid = g_variant_new("(uus)", 123, 456, "Invalid data");
    try
    {
        NetCfgChangeEvent invalid_event(invalid);
        std::cout << "FAILED: Data was parsed: " << invalid << std::endl;
        ++ret;
    }
    catch (const NetCfgException& excp)
    {
        std::cout << "PASSED: " << excp.what() << std::endl;
    }
    catch (const std::exception& excp)
    {
        std::cout << "FAILED: Unknown error: " << excp.what() << std::endl;
        ++ret;
    }
    g_variant_unref(invalid);
    return ret;
}


int process_filtermask(uint16_t m, uint8_t expect)
{
    std::vector<std::string> res = NetCfgChangeEvent::FilterMaskList(m, true);
    std::cout << "[" << std::to_string(res.size()) << " elements] ";
    for (const auto& t : res)
    {
        std::cout << t << " ";
    }
    if (res.size() != expect)
    {
        std::cout << " ... FAILED: Expected "<< std::to_string(expect)
                  << " elements" << std::endl;
        return 1;
    }
    std::cout << " ... PASSED" << std::endl;
    return 0;
}


int test_filtermasklist()
{

    int ret = 0;

    try
    {
        uint16_t mask = (1 << 16) - 1;
        std::cout << "-- Filter mask to string conversion (all bits set): ";

        ret += process_filtermask(mask, 16);

        mask = 2 | 16 | 2048 | 8192;
        std::cout << "-- Filter mask to string conversion (bits 1, 4, 11, 13 set): ";
        ret += process_filtermask(mask, 4);

        mask = 0;
        std::cout << "-- Filter mask to string conversion (no bits set): ";
        ret += process_filtermask(mask, 0);
    }
    catch (std::exception& excp)
    {
        std::cout << " ... FAILED: " << excp.what() << std::endl;
        ++ret;

    }
    return ret;
}

int main(int argc, char **argv)
{
    bool failed = false;
    int r = 0;

    std::cout << "** NetCfgChangeEvent Unit Test **" << std::endl;
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

    if ((r = test_filtermasklist()) > 0)
    {
        std::cout << "** test_filtermasklist() failed" << std::endl;
        failed = true;
    }

    std::cout << std::endl
              << ">> OVERAL TEST RESULT: " << (failed ? "FAILED" : "PASSED")
              << std::endl << std::endl;
    return (failed ? 2: 0);
}
