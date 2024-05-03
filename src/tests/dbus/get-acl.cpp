//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   getconfig-acl.cpp
 *
 * @brief  Simple test of the GetOwner() and GetAccessList() methods in
 *         the OpenVPN3ConfigurationProxy
 */

#include <iostream>
#include <exception>
#include <memory>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/object/path.hpp>

#include "common/lookup.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"


class ProxyWrangler
{
  public:
    ProxyWrangler(const DBus::Object::Path &objpath)
    {
        conn = DBus::Connection::Create(DBus::BusType::SYSTEM);
        if ("/net/openvpn/v3/configuration/" == objpath.substr(0, 30))
        {
            cfgprx = std::make_shared<OpenVPN3ConfigurationProxy>(conn, objpath);
        }
        else if ("/net/openvpn/v3/sessions/" == objpath.substr(0, 25))
        {
            auto sessmgr = SessionManager::Proxy::Manager::Create(conn);
            sessprx = sessmgr->Retrieve(objpath);
        }
        else
        {
            throw std::runtime_error("Unsuported object path");
        }
    }

    ~ProxyWrangler() noexcept = default;

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
    DBus::Connection::Ptr conn = nullptr;
    std::shared_ptr<OpenVPN3ConfigurationProxy> cfgprx = nullptr;
    SessionManager::Proxy::Session::Ptr sessprx = nullptr;
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

    if ("list" == mode)
    {
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
