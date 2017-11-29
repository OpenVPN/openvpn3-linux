//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, version 3 of the License
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef OPENVPN3_DBUS_CONNECTION_CREDS_HPP
#define OPENVPN3_DBUS_CONNECTION_CREDS_HPP

#include <vector>
#include <algorithm>
#include <sys/types.h>

#include "proxy.hpp"

using namespace openvpn;

namespace openvpn
{
    class DBusConnectionCreds : public DBusProxy
    {
    public:
        DBusConnectionCreds(GDBusConnection *dbuscon)
            : DBusProxy(dbuscon, "org.freedesktop.DBus", "org.freedesktop.DBus",
                        "/net/freedesktop/DBus", true)
        {
            SetGDBusCallFlags(G_DBUS_CALL_FLAGS_NO_AUTO_START);
            proxy = SetupProxy();
        }


        DBusConnectionCreds(DBus& dbusobj)
            : DBusProxy(dbusobj, "org.freedesktop.DBus", "org.freedesktop.DBus",
                        "/net/freedesktop/DBus", true)
        {
            SetGDBusCallFlags(G_DBUS_CALL_FLAGS_NO_AUTO_START);
            proxy = SetupProxy();
        }


        uid_t GetUID(std::string busname)
        {
            try
            {
                GVariant *result = Call("GetConnectionUnixUser",
                                      g_variant_new("(s)", busname.c_str()));
                uid_t ret;
                g_variant_get(result, "(u)", &ret);
                g_variant_unref(result);
                return ret;
            }
            catch (DBusException& excp)
            {
                THROW_DBUSEXCEPTION("DBusConnectionCreds",
                                    "Failed to retrieve UID for bus name '"
                                    + busname + "': " + excp.getRawError());
            }
        }


        pid_t GetPID(std::string busname)
        {
            try
            {
                GVariant *result = Call("GetConnectionUnixProcessID",
                                      g_variant_new("(s)", busname.c_str()));
                pid_t ret;
                g_variant_get(result, "(u)", &ret);
                g_variant_unref(result);
                return ret;
            }
            catch (DBusException& excp)
            {
                THROW_DBUSEXCEPTION("DBusConnectionCreds",
                                    "Failed to retrieve process ID for bus name '"
                                    + busname + "': " + excp.getRawError());
            }
        }
    };


    class DBusCredentialsException : public std::exception
    {
    public:
        DBusCredentialsException(uid_t requester, std::string quarkdomain, std::string error)
            : requester(requester), quarkdomain(quarkdomain), error(error)
        {
            std::stringstream s;
            s << error << " (Requester UID "<< requester << ")";
            error_uid = s.str();
        }

        virtual ~DBusCredentialsException() throw() {}

        virtual const char* what() const throw()
        {
            std::stringstream ret;
            ret << "[DBusCredentialsException"
                << "] " << error;
            return ret.str().c_str();
        }

        const std::string& err() const noexcept
        {
            return std::move(error_uid);
        }

        const std::string getUserError() const noexcept
        {
            return std::move(error);
        }

        void SetDBusError(GDBusMethodInvocation *invocation)
        {
            std::string qdom = (!quarkdomain.empty() ? quarkdomain : "net.openvpn.v3.error.undefined");
            GError *dbuserr = g_dbus_error_new_for_dbus_error(qdom.c_str(), error.c_str());
            g_dbus_method_invocation_return_gerror(invocation, dbuserr);
            g_error_free(dbuserr);
        }

        void SetDBusError(GError **dbuserror, GQuark domain, gint code)
        {
            g_set_error (dbuserror, domain, code, error.c_str());
        }

private:
        uid_t requester;
        std::string quarkdomain;
        std::string error;
        std::string error_uid;
    };



    class DBusCredentials : public DBusConnectionCreds
    {
    public:
        DBusCredentials(GDBusConnection *dbuscon, uid_t owner)
            : DBusConnectionCreds(dbuscon),
              owner(owner),
              acl_public(false)
        {
        }


        DBusCredentials(DBus& dbusobj, uid_t owner)
            : DBusConnectionCreds(dbusobj),
              owner(owner),
              acl_public(false)
        {
        }

        GVariant * GetOwner()
        {
            return g_variant_new_uint32(owner);
        }


        void SetPublicAccess(bool public_access)
        {
            acl_public = public_access;
        }


        GVariant * GetPublicAccess()
        {
            return g_variant_new_boolean(acl_public);
        }


        GVariant * GetAccessList()
        {
            GVariant *ret = NULL;
            GVariantBuilder *bld = g_variant_builder_new(G_VARIANT_TYPE("au"));
            for (auto& e : acl_list)
            {
                g_variant_builder_add(bld, "u", e);
            }
            ret = g_variant_builder_end(bld);
            g_variant_builder_unref(bld);

            return ret;
        }

        void GrantAccess(uid_t uid)
        {
            for (auto& acl_uid : acl_list)
            {
                if (acl_uid == uid)
                {
                    throw DBusCredentialsException(owner,
                                                   "net.openvpn.v3.error.acl.duplicate",
                                                   "UID already granted access");
                }
            }

            acl_list.push_back(uid);
        }


        void RevokeAccess(uid_t uid)
        {
            for (auto& acl_uid : acl_list)
            {
                if (acl_uid == uid)
                {
                    acl_list.erase(std::remove(acl_list.begin(), acl_list.end(), uid), acl_list.end());
                    return;
                }
            }

            throw DBusCredentialsException(owner,
                                           "net.openvpn.v3.error.acl.nogrant",
                                           "UID is not listed in access list");
        }

        void CheckACL(const std::string sender, bool allow_root = false)
        {
            check_acl(sender, false, allow_root);
        }


        void CheckOwnerAccess(const std::string sender, bool allow_root = false)
        {
            check_acl(sender, true, allow_root);
        }


    private:
        uid_t owner;
        bool acl_public;
        std::vector<uid_t> acl_list;


        void check_acl(const std::string sender, bool owner_only, bool allow_root)
        {
            if (acl_public && !owner_only)
            {
                return;
            }

            uid_t sender_uid = GetUID(sender);

            if (sender_uid == owner)
            {
                return;
            }

            if (allow_root && sender_uid == 0)
            {
                return;
            }

            if (owner_only)
            {
                throw DBusCredentialsException(sender_uid,
                                               "net.openvpn.v3.error.acl.denied",
                                               "Owner access denied"
                                               );
            }

            for (auto& acl_uid : acl_list)
            {
                if (acl_uid == sender_uid)
                {
                    return;
                }
            }
            throw DBusCredentialsException(sender_uid,
                                           "net.openvpn.v3.error.acl.denied",
                                           "Access denied");
        }
    };
};
#endif // OPENVPN3_DBUS_CONNECTION_CREDS_HPP
