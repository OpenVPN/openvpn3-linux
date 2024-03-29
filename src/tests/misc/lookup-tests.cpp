//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   lookup-tests.cpp
 *
 * @brief  Simple unit tests which checks lookup_uid(), lookup_username() and
 *         isanum_string()
 */

#include <iostream>

#include "common/utils.hpp"
#include "common/lookup.hpp"

int main(int argc, char **argv)
{
    if (1 == argc)
    {
        std::cout << ">> Username -> UID lookups" << std::endl;
        uid_t root = lookup_uid("root");
        uid_t nobody = lookup_uid("nobody");

        bool nonexist_fail = false;
        try
        {
            (void)lookup_uid("nonexiting_user");
        }
        catch (const LookupException &excp)
        {
            nonexist_fail = true;
        }

        std::cout << "     root uid: " << root << std::endl;
        std::cout << "   nobody uid: " << nobody << std::endl;
        std::cout << "      invalid: "
                  << (nonexist_fail ? "(correct, not found)" : "**ERROR**")
                  << std::endl;
        std::cout << std::endl;

        std::cout << ">> UID -> username lookups" << std::endl;
        std::string root_username = lookup_username(root);
        std::string nobody_username = lookup_username(nobody);
        std::cout << "   Lookup UID " << root << ": username "
                  << root_username << std::endl;
        std::cout << "   Lookup UID " << nobody << ": username "
                  << nobody_username << std::endl;
        std::cout << std::endl;

        if (("root" == root_username)
            && ("nobody" == nobody_username)
            && nonexist_fail)
        {
            std::cout << "** Result: All tests passed" << std::endl;
            return 0;
        }
        std::cout << "** Result: FAIL" << std::endl;
        return 1;
    }
    else
    {
        try
        {
            uid_t uid = get_userid(argv[1]);
            std::string username = lookup_username(uid);

            if ('(' != username[0])
            {
                std::cout << "Username '" << username << "'  <==>  uid " << uid
                          << std::endl;
            }
            else
            {
                std::cout << "UID " << uid << " was not found" << std::endl;
            }
        }
        catch (const LookupException &excp)
        {
            std::cout << "** ERROR ** " << excp.str() << std::endl;
        }
    }
    return 0;
}
