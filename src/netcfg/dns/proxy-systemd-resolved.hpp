//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2020         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020         David Sommerseth <davids@openvpn.net>
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
 * @file   proxy-systemd-resolved.hpp
 *
 * @brief  D-Bus proxy for the systemd-resolved service
 */

#pragma once

#include <string>
#include <exception>

#include "dbus/core.hpp"

using namespace openvpn;

namespace NetCfg {
namespace DNS {
namespace resolved {

    struct ResolverRecord
    {
        typedef std::vector<ResolverRecord> List;

        ResolverRecord(unsigned short family, std::string server);
        ResolverRecord(GVariant *entry);
        virtual ~ResolverRecord() = default;

        GVariant *GetGVariant() const;

        unsigned short family;
        std::string server;
    };


    struct SearchDomain
    {
        typedef std::vector<SearchDomain> List;

        SearchDomain(std::string srch, bool rout);
        SearchDomain(GVariant *entry);
        virtual ~SearchDomain() = default;

        GVariant *GetGVariant() const;

        std::string search;
        bool routing;
    };


    class Exception : public std::exception
    {
    public:
        Exception(const std::string& err)
            : errmsg(err)
        {
        }
        virtual ~Exception() = default;

        virtual const char* what() const noexcept
        {
            return errmsg.c_str();
        }

    private:
        std::string errmsg;
    };


    class Link : public DBusProxy,
                 public virtual RC<thread_unsafe_refcount>
    {
    public:
        typedef RCPtr<Link> Ptr;

        Link(GDBusConnection* dbuscon, const std::string path);
        virtual ~Link() = default;

        const std::string GetPath() const;
        const std::vector<std::string> GetDNSServers() const;
        void SetDNSServers(const ResolverRecord::List& servers) const;
        const std::string GetCurrentDNSServer() const;
        const SearchDomain::List GetDomains() const;
        void SetDomains(const SearchDomain::List& doms) const;
        bool GetDefaultRoute() const;
        void SetDefaultRoute(const bool route) const;
        void Revert() const;
    };


    class Manager : public DBusProxy
    {
    public:
        Manager(GDBusConnection* dbuscon);
        virtual ~Manager() = default;

        Link::Ptr RetrieveLink(const std::string dev_name);

        std::string GetLink(unsigned int if_idx);
    };
} // namespace resolved
} // namespace DNS
} // namespace NetCfg
