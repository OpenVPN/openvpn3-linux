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
 * @file   getconfig-acl.cpp
 *
 * @brief  Simple test of the GetOwner() and GetAccessList() methods in
 *         the OpenVPN3ConfigurationProxy
 */

#include <iostream>
#include <exception>

#include "dbus/core.hpp"
#include "common/lookup.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"

using namespace openvpn;

class ProxyWrangler
{
public:
    ProxyWrangler(std::string objpath)
    {
        if ("/net/openvpn/v3/configuration/" == objpath.substr(0,30))
        {
            cfgprx = new OpenVPN3ConfigurationProxy(G_BUS_TYPE_SYSTEM, objpath);
        }
        else if ("/net/openvpn/v3/sessions/" == objpath.substr(0,25))
        {
            sessprx = new OpenVPN3SessionProxy(G_BUS_TYPE_SYSTEM, objpath);
        }
        else
        {
            throw std::runtime_error("Unsuported object path");
        }
    }

    ~ProxyWrangler()
    {
        if (nullptr != cfgprx)
        {
            delete cfgprx;
        }
        if (nullptr != sessprx)
        {
            delete sessprx;
        }
    }

    uid_t GetOwner()
    {
        if (nullptr != cfgprx)
        {
            return cfgprx->GetOwner();
        }
        if (nullptr != sessprx)
        {
            return sessprx->GetOwner();
        }
        throw std::runtime_error("Unsupported operational mode");
    }

    std::vector<uid_t> GetAccessList()
    {
        if (nullptr != cfgprx)
        {
            return cfgprx->GetAccessList();
        }
        if (nullptr != sessprx)
        {
            return sessprx->GetAccessList();
        }
        throw std::runtime_error("Unsupported operational mode");
    }


    void AccessGrant(uid_t uid)
    {
        if (nullptr != cfgprx)
        {
            return cfgprx->AccessGrant(uid);
        }
        if (nullptr != sessprx)
        {
            return sessprx->AccessGrant(uid);
        }
        throw std::runtime_error("Unsupported operational mode");
    }


    void AccessRevoke(uid_t uid)
    {
        if (nullptr != cfgprx)
        {
            return cfgprx->AccessRevoke(uid);
        }
        if (nullptr != sessprx)
        {
            return sessprx->AccessRevoke(uid);
        }
        throw std::runtime_error("Unsupported operational mode");
    }


private:
    OpenVPN3ConfigurationProxy * cfgprx = nullptr;
    OpenVPN3SessionProxy * sessprx = nullptr;
};


int main(int argc, char **argv)
{
    if (3 > argc)
    {
        std::cout << "Usage: " << argv[0] << " <list|grant|revoke> <object path> [UID]"
                  << std::endl;
        return 1;
    }

    std::string mode(argv[1]);
    ProxyWrangler proxy(argv[2]);

    if ("list" == mode) {
        std::cout << "Object owner: (" << proxy.GetOwner()
                  << ") " << lookup_username(proxy.GetOwner())
                  << std::endl;
        std::cout << "Users granted access: " << std::endl;
        for (uid_t uid : proxy.GetAccessList())
        {
            std::cout << "    uid: " << uid
                      << "  user: " << lookup_username(uid)
                      << std::endl;
        }
    }
    else if ("grant" == mode)
    {
        if (4 > argc)
        {
            std::cerr << "Missing UID to grant access to" << std::endl;
            return 1;
        }
        proxy.AccessGrant(get_userid(argv[3]));
        std::cout << "Access granted" << std::endl;
    }
    else if ("revoke" == mode)
    {
        if (4 > argc)
        {
            std::cerr << "Missing UID to revoke access from" << std::endl;
            return 1;
        }

        proxy.AccessRevoke(get_userid(argv[3]));
        std::cout << "Access revoked" << std::endl;
    }
    else
    {
        std::cerr << "Unknown mode.  First argument must me list, grant or revoke"
                  << std::endl;
        return 2;
    }
    return 0;
}
