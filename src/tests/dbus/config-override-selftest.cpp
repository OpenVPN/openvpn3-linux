//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   config-override-selftest.cpp
 *
 * @brief  Imports a minimalistic configuration profile and tests the various
 *         overrides available
 */

#include <exception>
#include <iostream>
#include <vector>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/object/path.hpp>

#include "dbus/constants.hpp"
#include "configmgr/proxy-configmgr.hpp"



bool check_override_value(const Override ov, bool expect)
{
    if (!std::holds_alternative<bool>(ov.value))
    {
        return false;
    }

    return expect == std::get<bool>(ov.value);
}


bool check_override_value(const Override ov, std::string expect)
{
    if (!std::holds_alternative<std::string>(ov.value))
    {
        return false;
    }

    return expect == std::get<std::string>(ov.value);
}


int main(int argc, char **argv)
{
    DBus::BusType bus = DBus::BusType::SYSTEM;
    if (argc > 1 && std::string(argv[1]) == "--use-session-bus")
    {
        bus = DBus::BusType::SESSION;
    }

    auto conn = DBus::Connection::Create(bus);

    OpenVPN3ConfigurationProxy::Ptr cfgmgr = nullptr;
    try
    {
        cfgmgr = OpenVPN3ConfigurationProxy::Create(conn,
                                                    Constants::GenPath("configuration"));
        cfgmgr->Ping();
        if (!cfgmgr->CheckObjectExists())
        {
            throw std::runtime_error("Service unavailable");
        }
    }
    catch (const std::exception &)
    {
        // Skip this test if the config manager service is unavailable
        return 77;
    }

    std::stringstream profile;
    profile << "dev tun" << std::endl
            << "remote localhost" << std::endl
            << "client" << std::endl;
    DBus::Object::Path cfgpath = cfgmgr->Import("selftest:overrides",
                                                profile.str(),
                                                false,
                                                false);

    OpenVPN3ConfigurationProxy cfgobj(conn, cfgpath);
    unsigned int failed = 0;

    std::cout << ".. Testing unsetting an unset override ... ";
    try
    {
        auto ov = GetConfigOverride("ipv6");
        cfgobj.UnsetOverride(*ov);
        std::cout << "FAIL" << std::endl;
        ++failed;
    }
    catch (const DBus::Exception &excp)
    {
        std::string e(excp.what());
        if (e.find("Override 'ipv6' has not been set") != std::string::npos)
        {
            std::cout << "PASS" << std::endl;
        }
        else
        {
            std::cout << "ERROR:" << excp.what() << std::endl;
            ++failed;
        }
    }

    std::cout << ".. Testing unsetting an invalid override ... ";
    try
    {
        Override ov = {"non-existing-override",
                       std::string{},
                       "Non-existing override"};
        cfgobj.UnsetOverride(ov);
        std::cout << "FAIL" << std::endl;
        ++failed;
    }
    catch (const DBus::Exception &excp)
    {
        std::string e(excp.what());
        if (e.find("Override 'non-existing-override' has not been set") != std::string::npos)
        {
            std::cout << "PASS" << std::endl;
        }
        else
        {
            std::cout << "ERROR:" << excp.what() << std::endl;
            ++failed;
        }
    }

    std::cout << ".. Testing setting an invalid override ... ";
    try
    {
        const Override ov = {"non-existing-override",
                             std::string{},
                             "Non-existing override"};
        cfgobj.SetOverride(ov, std::string("string-value"));
        std::cout << "FAIL" << std::endl;
        ++failed;
    }
    catch (const DBus::Exception &excp)
    {
        std::string e(excp.what());
        if (e.find("Invalid override key 'non-existing-override'") != std::string::npos)
        {
            std::cout << "PASS" << std::endl;
        }
        else
        {
            std::cout << "ERROR:" << excp.what() << std::endl;
            ++failed;
        }
    }

    std::cout << ".. Testing setting an override with invalid type [bool:string] (1) ... ";
    try
    {
        auto ov = GetConfigOverride("dns-sync-lookup");
        cfgobj.SetOverride(*ov, std::string("string-value"));
        std::cout << "FAIL" << std::endl;
        ++failed;
    }
    catch (const DBus::Exception &excp)
    {
        std::string e(excp.what());
        if (e.find("Invalid override data type for 'dns-sync-lookup'") != std::string::npos)
        {
            std::cout << "PASS" << std::endl;
        }
        else
        {
            std::cout << "ERROR:" << excp.what() << std::endl;
            ++failed;
        }
    }

    std::cout << ".. Testing setting an override with invalid type [bool:string] (2) ... ";
    try
    {
        auto ov = GetConfigOverride("dns-sync-lookup");
        ov->value = std::string{};
        cfgobj.SetOverride(*ov, std::string("string-value"));
        std::cout << "FAIL" << std::endl;
        ++failed;
    }
    catch (const DBus::Exception &excp)
    {
        std::string e(excp.what());
        if (e.find("Invalid override data type for 'dns-sync-lookup':") != std::string::npos)
        {
            std::cout << "PASS" << std::endl;
        }
        else
        {
            std::cout << "ERROR:" << excp.what() << std::endl;
            ++failed;
        }
    }

    std::cout << ".. Testing setting an override with invalid type [string:bool] (1) ... ";
    try
    {
        auto ov = GetConfigOverride("server-override");
        cfgobj.SetOverride(*ov, true);
        std::cout << "FAIL" << std::endl;
        ++failed;
    }
    catch (const DBus::Exception &excp)
    {
        std::string e(excp.what());
        if (e.find("Invalid override data type for 'server-override'") != std::string::npos)
        {
            std::cout << "PASS" << std::endl;
        }
        else
        {
            std::cout << "ERROR:" << excp.what() << std::endl;
            ++failed;
        }
    }

    std::cout << ".. Testing setting an override with invalid type [string:bool] (2) ... ";
    try
    {
        const Override ov = {"server-override",
                             false,
                             "Non-existing override"};
        cfgobj.SetOverride(ov, true);
        std::cout << "FAIL" << std::endl;
        ++failed;
    }
    catch (const DBus::Exception &excp)
    {
        std::string e(excp.what());
        if (e.find("Invalid override data type for 'server-override'") != std::string::npos)
        {
            std::cout << "PASS" << std::endl;
        }
        else
        {
            std::cout << "ERROR:" << excp.what() << std::endl;
            ++failed;
        }
    }

    std::cout << ".. Testing setting a string override with ASCII control chars (need to be removed by service) ... ";
    try
    {
        const Override ov = {"server-override",
                             true,
                             "Value containing ctrl chars"};
        cfgobj.SetOverride(ov, std::string("abc\x01\x02\x03\ndef\n\nghi\r\n"));
        auto chk = cfgobj.GetOverrideValue(ov.key);
        if (std::string("abcdefghi") == std::get<std::string>(chk.value))
        {
            std::cout << "PASS" << std::endl;
        }
        else
        {
        std::cout << "FAIL" << std::endl;
        ++failed;
        }
        cfgobj.UnsetOverride(ov);
    }
    catch (const DBus::Exception &excp)
    {
        std::string e(excp.what());
        std::cout << "EXCEPTION: " << excp.what() << std::endl;
        ++failed;
    }


    std::cout << ".. Checking all overrides are unset ... ";
    std::vector<Override> overrides = cfgobj.GetOverrides();
    if (0 != overrides.size())
    {
        ++failed;
        std::cout << "FAIL";
    }
    else
    {
        std::cout << "PASS";
    }
    std::cout << std::endl;


    // Set all overrides to some values
    for (auto &cfgoverride : configProfileOverrides)
    {
        if (std::holds_alternative<std::string>(cfgoverride.value))
        {
            std::cout << ".. Setting override: [type string] "
                      << cfgoverride.key << " = '"
                      << "override:" + cfgoverride.key
                      << "'" << std::endl;
            cfgobj.SetOverride(cfgoverride, "override:" + cfgoverride.key);
        }
        else
        {
            std::cout << ".. Setting override: [type boolean] "
                      << cfgoverride.key << " = true" << std::endl;
            cfgobj.SetOverride(cfgoverride, true);
        }
    }

    // Get all the overrides, and check if the value is what we expects
    std::vector<Override> chkov = cfgobj.GetOverrides();
    for (const auto &cfgoverride : configProfileOverrides)
    {
        std::cout << ".. Checking override '" << cfgoverride.key << "': ";
        std::string failmsg = "";
        for (const auto &cov : chkov)
        {
            if (cov.key != cfgoverride.key)
            {
                continue;
            }

            if (cov.value.index() != cfgoverride.value.index())
            {
                failmsg = "FAIL - Type mismatch";
                break;
            }
            else if (std::holds_alternative<std::string>(cov.value))
            {
                std::string expect = "override:" + cov.key;
                if (!check_override_value(cov, expect))
                {
                    failmsg = "FAIL - incorrect override string value";
                }
                break;
            }
            else
            {
                if (!check_override_value(cov, true))
                {
                    failmsg = "FAIL - incorrect override boolean value";
                }
                break;
            }
        }
        if (failmsg.empty())
        {
            std::cout << "PASS" << std::endl;
        }
        else
        {
            ++failed;
            std::cout << failmsg << std::endl;
        }
    }

    if (0 == failed)
    {
        // Unset all overrides
        for (const auto &cfgoverride : configProfileOverrides)
        {
            std::cout << ".. Unsetting override: " << cfgoverride.key
                      << std::endl;
            cfgobj.UnsetOverride(cfgoverride);
        }

        // Check all overrides are once again unset
        std::cout << ".. Checking all unset overrides are unset ... ";
        std::vector<Override> unset = cfgobj.GetOverrides();
        if (0 != unset.size())
        {
            ++failed;
            std::cout << "FAIL";
        }
        else
        {
            std::cout << "PASS";
        }
        std::cout << std::endl;
        cfgobj.Remove();
    }

    std::cout << std::endl;
    std::cout << "OVERALL TEST RESULT: "
              << (0 == failed ? "PASS"
                              : std::string("FAIL ("
                                            + std::to_string(failed))
                                    + " failed)")
              << std::endl;

    return (0 == failed ? 0 : 2);
}
