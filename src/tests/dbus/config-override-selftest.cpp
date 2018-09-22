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
 * @file   config-override-selftest.cpp
 *
 * @brief  Imports a minimalistic configuration profile and tests the various
 *         overrides available
 */

#include <iostream>
#include <vector>

#include "dbus/core.hpp"
#include "configmgr/proxy-configmgr.hpp"

using namespace openvpn;


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
        DBus dbusobj(G_BUS_TYPE_SYSTEM);
        dbusobj.Connect();

        OpenVPN3ConfigurationProxy cfgmgr(dbusobj,
                                          OpenVPN3DBus_rootp_configuration);
        cfgmgr.Ping();

        std::stringstream profile;
        profile << "dev tun" << std::endl
                << "remote localhost" << std::endl
                << "client" << std::endl;
        std::string cfgpath = cfgmgr.Import("selftest:overrides",
                                            profile.str(), false, false);

        OpenVPN3ConfigurationProxy cfgobj(dbusobj, cfgpath);
        unsigned int failed = 0;

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
        for (auto & cfgoverride : configProfileOverrides)
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
        for (const auto& cfgoverride : configProfileOverrides)
        {
            std::cout << ".. Checking override '" << cfgoverride.key << "': ";
            std::string failmsg = "";
            for (const auto& cov : chkov)
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
                    failmsg = "FAIL - Unkown override type";
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
            for (const auto& cfgoverride : configProfileOverrides)
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



