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

#include <iostream>
#include <vector>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/object/path.hpp>

#include "dbus/constants.hpp"
#include "configmgr/proxy-configmgr.hpp"



bool check_override_value(const OverrideValue ov, OverrideType ovt, bool expect)
{
    if (false == ov.override.valid())
    {
        return false;
    }

    if (ovt != ov.override.type)
    {
        return false;
    }

    if (expect != ov.boolValue)
    {
        return false;
    }
    return true;
}


bool check_override_value(const OverrideValue ov, OverrideType ovt, std::string expect)
{
    if (false == ov.override.valid())
    {
        return false;
    }

    if (ovt != ov.override.type)
    {
        return false;
    }

    if (expect != ov.strValue)
    {
        return false;
    }
    return true;
}


int main(int argc, char **argv)
{
    auto conn = DBus::Connection::Create(DBus::BusType::SYSTEM);

    OpenVPN3ConfigurationProxy cfgmgr(conn,
                                      Constants::GenPath("configuration"));
    cfgmgr.Ping();

    std::stringstream profile;
    profile << "dev tun" << std::endl
            << "remote localhost" << std::endl
            << "client" << std::endl;
    DBus::Object::Path cfgpath = cfgmgr.Import("selftest:overrides",
                                               profile.str(),
                                               false,
                                               false);

    OpenVPN3ConfigurationProxy cfgobj(conn, cfgpath);
    unsigned int failed = 0;

    std::cout << ".. Testing unsetting an unset override ... ";
    try
    {
        const ValidOverride ov = GetConfigOverride("ipv6");
        cfgobj.UnsetOverride(ov);
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

    std::cout << ".. Testing unsetting an invalid override (1)... ";
    try
    {
        const ValidOverride ov = GetConfigOverride("non-existent-fake-override");
        cfgobj.UnsetOverride(ov);
        std::cout << "FAIL" << std::endl;
        ++failed;
    }
    catch (const DBus::Exception &excp)
    {
        std::string e(excp.what());
        if (e.find("Invalid override") != std::string::npos)
        {
            std::cout << "PASS" << std::endl;
        }
        else
        {
            std::cout << "ERROR:" << excp.what() << std::endl;
            ++failed;
        }
    }

    std::cout << ".. Testing unsetting an invalid override (2)... ";
    try
    {
        ValidOverride ov = {"non-existing-override",
                            OverrideType::string,
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

    std::cout << ".. Testing setting an invalid override (1)... ";
    try
    {
        const ValidOverride ov = GetConfigOverride("non-existent-fake-override");
        cfgobj.SetOverride(ov, "string-value");
        std::cout << "FAIL" << std::endl;
        ++failed;
    }
    catch (const DBus::Exception &excp)
    {
        std::string e(excp.what());
        if (e.find("Invalid override") != std::string::npos)
        {
            std::cout << "PASS" << std::endl;
        }
        else
        {
            std::cout << "ERROR:" << excp.what() << std::endl;
            ++failed;
        }
    }

    std::cout << ".. Testing setting an invalid override (2)... ";
    try
    {
        const ValidOverride ov = {"non-existing-override",
                                  OverrideType::string,
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
        const ValidOverride ov = GetConfigOverride("dns-sync-lookup");
        cfgobj.SetOverride(ov, std::string("string-value"));
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
        ValidOverride ov = GetConfigOverride("dns-sync-lookup");
        ov.type = OverrideType::string;
        cfgobj.SetOverride(ov, std::string("string-value"));
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
        const ValidOverride ov = GetConfigOverride("server-override");
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

    std::cout << ".. Testing setting an override with invalid type [string:bool] (2) ... ";
    try
    {
        const ValidOverride ov = {"server-override",
                                  OverrideType::boolean,
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


    std::cout << ".. Checking all overrides are unset ... ";
    std::vector<OverrideValue> overrides = cfgobj.GetOverrides();
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
        if (OverrideType::string == cfgoverride.type)
        {
            std::cout << ".. Setting override: [type string] "
                      << cfgoverride.key << " = '"
                      << "override:" + cfgoverride.key
                      << "'" << std::endl;
            cfgobj.SetOverride(cfgoverride, "override:" + cfgoverride.key);
        }
        else if (OverrideType::boolean == cfgoverride.type)
        {
            std::cout << ".. Setting override: [type boolean] "
                      << cfgoverride.key << " = true" << std::endl;
            cfgobj.SetOverride(cfgoverride, true);
        }
        else
        {
            ++failed;
            std::cout << "!! Unknown data type for override key "
                      << cfgoverride.key << std::endl;
        }
    }

    // Get all the overrides, and check if the value is what we expects
    std::vector<OverrideValue> chkov = cfgobj.GetOverrides();
    for (const auto &cfgoverride : configProfileOverrides)
    {
        std::cout << ".. Checking override '" << cfgoverride.key << "': ";
        std::string failmsg = "";
        for (const auto &cov : chkov)
        {
            if (cov.override.key != cfgoverride.key)
            {
                continue;
            }

            if (cov.override.type != cfgoverride.type)
            {
                failmsg = "FAIL - Type mismatch";
                break;
            }
            else if (OverrideType::string == cov.override.type)
            {
                std::string expect = "override:" + cov.override.key;
                if (!check_override_value(cov, OverrideType::string, expect))
                {
                    failmsg = "FAIL - incorrect override string value";
                }
                break;
            }
            else if (OverrideType::boolean == cov.override.type)
            {
                if (!check_override_value(cov, OverrideType::boolean, true))
                {
                    failmsg = "FAIL - incorrect override boolean value";
                }
                break;
            }
            else
            {
                failmsg = "FAIL - Unknown override type";
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
        std::vector<OverrideValue> unset = cfgobj.GetOverrides();
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
