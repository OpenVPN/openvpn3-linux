//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017 - 2022  OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017 - 2022  David Sommerseth <davids@openvpn.net>
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
 * @file   connection-creds.hpp
 *
 * @brief  Implements authorization control which can be used by DBusObject
 *         objects.  This implementation covers all aspects of identifying
 *         a D-Bus callers credentials to managing access control lists (ACL)
 *         and authorizing D-Bus callers against the ACL.
 */

#pragma once

#include <vector>
#include <algorithm>
#include <sys/types.h>

#include "common/lookup.hpp"
#include "proxy.hpp"

/**
 *   Queries the D-Bus daemon for the credentials of a specific D-Bus
 *   bus name.  Each D-Bus client performing an operation on a D-Bus
 *   object in a service connects with a unique bus name.  This is a
 *   safe method for retrieving information about who the caller is.
 */
class DBusConnectionCreds : public DBusProxy
{
public:
    /**
     * Initiate the object for querying the D-Bus daemon.  This is always
     * the org.freedesktop.DBus service and the interface is also the
     * same value.  Object path is not used by this service.
     *
     * @param dbuscon  An established D-Bus connection to use for queries
     */
    DBusConnectionCreds(GDBusConnection *dbuscon)
        : DBusProxy(dbuscon, "org.freedesktop.DBus", "org.freedesktop.DBus",
                    "/net/freedesktop/DBus", true)
    {
        SetGDBusCallFlags(G_DBUS_CALL_FLAGS_NO_AUTO_START);
        proxy = SetupProxy();
    }


    /**
     *  Retrieve the UID of the owner of a specific bus name
     *
     * @param  busname  String containing the bus name
     * @return Returns an uid_t containing the bus owners UID.
     *         In case of errors, a DBusException is thrown.
     */
    uid_t GetUID(std::string busname) const
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
                                + busname + "': " + excp.GetRawError());
        }
    }


    /**
     *  Retrieves the bus name callers process ID (PID)
     *
     * @param busname  String containing the bus name for the query
     * @return Returns a pid_t value of the process ID.
     *         In case of errors, a DBusException is thrown.
     */
    pid_t GetPID(std::string busname) const
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
                                + busname + "': " + excp.GetRawError());
        }
    }


    /**
     *  Looks up the unique D-Bus bus name for a given D-Bus service,
     *  based on a well-known bus name.
     *
     * @param busname A string containing the well known bus name.
     *
     * @return Returns a string containing the unique bus name if found,
     *         otherwise a DBusException is thrown.
     */
    std::string GetUniqueBusID(std::string busname) const
    {
        try
        {
            GVariant *result = Call("GetNameOwner",
                                  g_variant_new("(s)", busname.c_str()));
            gchar *res = nullptr;
            g_variant_get(result, "(s)", &res);
            g_variant_unref(result);
            auto ret = std::string(res);
            g_free(res);
            return ret;
        }
        catch (DBusException& excp)
        {
            THROW_DBUSEXCEPTION("DBusConnectionCreds",
                                "Failed to retrieve unique bus ID for bus name '"
                                + busname + "': " + excp.GetRawError());
        }
    }
};


/**
 *  Exception class used to identify authorization errors
 */
class DBusCredentialsException : public std::exception
{
public:
    /**
     *  Initiate the authorization failure exception
     * @param requester    uid_t containing the rejected UID
     * @param quarkdomain  String which classifies the authorization
     *                     error
     * @param error        String containing the authorization failure
     *                     message
     */
    DBusCredentialsException(uid_t requester,
                             std::string quarkdomain, std::string error)
        : quarkdomain(quarkdomain), error(error)
    {
        std::stringstream s;
        s << error << " (Requester UID "<< requester << ")";
        error_uid = s.str();
    }


    /**
     *  Initiate the authorization failure exception.  This is
     *  a variant used when it is not possible to map the D-Bus sender
     *  to an UID.
     *
     * @param requester    String containing the rejected D-Bus sender
     * @param quarkdomain  String which classifies the authorization
     *                     error
     * @param error        String containing the authorization failure
     *                     message
     */
    DBusCredentialsException(std::string requester,
                             std::string quarkdomain, std::string error)
        : quarkdomain(quarkdomain), error(error)
    {
        std::stringstream s;
        s << error << " (D-Bus Request sender "<< requester << ")";
        error_uid = s.str();
    }

    virtual ~DBusCredentialsException() noexcept = default;

    virtual const char* what() const noexcept
    {
        return error.c_str();
    }

    /**
     *  Wrapper for more easily returning an authorization failure
     *  back to an on going D-Bus method call.  This will transport the
     *  error back to the D-Bus caller.
     *
     * @param invocation Pointer to a invocation object of the on-going
     *                   method call
     */
    void SetDBusError(GDBusMethodInvocation *invocation)
    {
        std::string qdom = (!quarkdomain.empty() ? quarkdomain : "net.openvpn.v3.error.undefined");
        GError *dbuserr = g_dbus_error_new_for_dbus_error(qdom.c_str(), error.c_str());
        g_dbus_method_invocation_return_gerror(invocation, dbuserr);
        g_error_free(dbuserr);
    }


    /**
     *  Wrapper for more easily returning an authorization failure back
     *  to an on going D-Bus set/get property call.  This will transport
     *  the error back to the D-Bus caller
     *
     * @param dbuserror  Pointer to a GError pointer where the error will
     *                   be saved
     * @param domain     Error Quark domain used to classify the
     *                   authentication failure
     * @param code       A GIO error code, used for further error
     *                   error classification.  Look up G_IO_ERROR_*
     *                   entries in glib-2.0/gio/gioenums.h for details.
     */
    void SetDBusError(GError **dbuserror, GQuark domain, gint code)
    {
        g_set_error (dbuserror, domain, code, "%s", error.c_str());
    }

private:
    std::string quarkdomain;
    std::string error;
    std::string error_uid;
};



/**
 *  Implements an access control list which contains user IDs (UID) of
 *  users allowed to get access.  If SetPublicAccess(true) is called,
 *  then the ACL check is skipped and everyone have access.  An owner
 *  will always have access, regardless of the ACL lists contents.
 */
class DBusCredentials : public DBusConnectionCreds
{
public:
    /**
     *   Initializes the ACL check object
     *
     * @param dbuscon  Established D-Bus connection to use for various
     *                 queries
     * @param owner    uid_t of the UID owning this object
     */
    DBusCredentials(GDBusConnection *dbuscon, uid_t owner)
        : DBusConnectionCreds(dbuscon),
          owner(owner),
          acl_public(false)
    {
    }


    /**
     *  Returns this objects owner's UID
     *
     * @return GVariant<uint32> object containing the UID
     */
    GVariant * GetOwner() const
    {
        return g_variant_new_uint32(owner);
    }


    /**
     *  Returns this objects owner's UID as uid_t
     *
     * @returns uid_t containing the object owner's uid
     */
    uid_t GetOwnerUID() const
    {
        return owner;
    }


    /**
     *  Transfer ownership to a new owner uid
     *
     * @param new_owner  uid_t containing the new owner
     */
    void TransferOwnership(const uid_t new_owner)
    {
        owner = new_owner;
    }


    /**
     *  Sets the public access attribute.  If set to true,
     *  the ACL check is effectively disabled - unless a
     *  strict owner-only check is performed.
     *
     * @param public_access  Boolean value enabling/disabling public
     *                       access
     */
    void SetPublicAccess(bool public_access)
    {
        acl_public = public_access;
    }


    /**
     *  Retrieves the public access attribute
     *
     * @return Returns a boolean with the public access attribute
     */
    bool GetPublicAccess() const
    {
        return acl_public;
    }


    /**
     *  Retrieve the ACL list of UIDs granted access.  The owner UID
     *  is not enlisted.
     *
     * @return  Returns a std::vector object containing an array of uid_t
     */
    std::vector<uid_t> GetAccessList() const
    {
        return acl_list;
    }

    /**
     *  Adds a user ID (UID) to the access list
     *
     * @param uid  uid_t containing the users UID granted access
     */
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


    /**
     *  Removes a user ID (UID) from the access list
     *
     * @param uid uid_t containing the users UID getting access revoked
     */
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


    /**
     *  Checks if a D-Bus callers UID is enlisted in the ACL for this
     *  object.  If the callers UID matches the owner UID, access is
     *  granted as well.  If the public access attribute is set to true,
     *  the access control is skipped and will always pass.
     *
     *  In case of authorization failure, a DBusCredentialsException
     *  is thrown.
     *
     * @param sender      String containing the callers D-Bus bus name
     * @param allow_mngr  Set to false by default.  If set to true, the
     *                    openvpn user (OpenVPN Manager) will be granted
     *                    access regardless of the UID.
     */
    void CheckACL(const std::string sender, bool allow_mngr = false) const
    {
        check_acl(sender, false, allow_mngr);
    }


    /**
     *  This is an extended CheckACL() method which grants the root user
     *  access regardless of the ACL.
     *
     *  In case of authorization failure, a DBusCredentialsException
     *  is thrown.
     *
     * @param sender      String containing the callers D-Bus bus name
     * @param allow_mngr  Set to false by default.  If set to true, the
     *                    openvpn user (OpenVPN Manager) will be granted
     *                    access regardless of the UID.
     */
    void CheckACL_allowRoot(const std::string sender, bool allow_mngr = false) const
    {
        if ( 0 == GetUID(sender) )
        {
            // Allow root access
            return;
        }
        check_acl(sender, false, allow_mngr);
    }


    /**
     *  Restricted access control, where only the owner of the object
     *  will be granted access.  This check will _NOT_ respect the
     *  public access attribute; as this check is often used when
     *  removing objects or otherwise doing more intrusive changes.
     *
     *  In case of authorization failure, a DBusCredentialsException
     *  is thrown.
     *
     * @param sender      String containing the callers D-Bus bus name
     * @param allow_mngr  Set to false by default.  If set to true, the
     *                    openvpn user (OpenVPN manager) will be granted
     *                    access regardless of the UID.
     */
    void CheckOwnerAccess(const std::string sender,
                          bool allow_mngr = false) const
    {
        check_acl(sender, true, allow_mngr);
    }


private:
    uid_t owner;
    bool acl_public;
    std::vector<uid_t> acl_list;


    /**
     *  Core authorization method.  It will retrieve the users UID based
     *  on the D-Bus senders bus name and verify that UID value against
     *  the ACL, the object owners UID, or even the openvpn user (OpenVPN
     *  Manager) if allowed.
     *
     *  If the public access attribute is set to true, authorization is
     *  skipped - but only for non-owner-only queries.  Owner-only checks
     *  will only succeed if the D-Bus caller is the object owner or
     *  openvpn user (if allowed)
     *
     *  In case of authorization failure, a DBusCredentialsException
     *  is thrown.
     *
     * @param sender      String containing the callers D-Bus bus name
     * @param owner_only  Only allow the owner of the object?
     * @param allow_mngr  Allow openvpn user (OpenVPN manager) regardless
     *                    of ACL?
     */
    void check_acl(const std::string sender,
                   bool owner_only, bool allow_mngr) const
    {
        if (acl_public && !owner_only)
        {
            return;
        }

        uid_t sender_uid = 65535;
        try
        {
            sender_uid = GetUID(sender);

            if (sender_uid == owner)
            {
                return;
            }

            uid_t ovpn_uid;
            ovpn_uid = lookup_uid(OPENVPN_USERNAME);

            if (allow_mngr && sender_uid == ovpn_uid)
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
        catch (const DBusCredentialsException&)
        {
            throw;
        }
        catch (const LookupException& excp)
        {
            throw DBusCredentialsException(sender_uid,
                                           "net.openvpn.v3.error.acl.lookup",
                                           excp.str());
        }
        catch (const DBusException& excp)
        {
            throw DBusCredentialsException(sender_uid,
                    "net.openvpn.v3.error.acl.lookup",
                    "Could not identify caller");
        }
    }
};
