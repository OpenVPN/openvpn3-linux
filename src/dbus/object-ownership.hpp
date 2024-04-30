//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017  David Sommerseth <davids@openvpn.net>
//

/**
 * @file object-ownership.hpp
 *
 * @brief Provides DBus::Object::Base ownership and ACL management
 */

#pragma once

#include <memory>
#include <string>
#include <sys/types.h>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/credentials/query.hpp>


namespace GDBusPP::Object::Extension {

using ACLList = std::vector<uid_t>;

class ACLException : public DBus::Object::Exception
{
  public:
    ACLException(const std::string &msg);
};


/**
 *  This can be used inside an object to track the owner of the DBus::Object
 *  in addition to the a simpler access control list (ACL) which can be
 *  accessed from the DBus::Object::Base::Authorize() method to control which
 *  calling users will be granted access.
 */
class ACL
{
  public:
    using Ptr = std::shared_ptr<ACL>;

    /**
     *  Create a new ACL object
     *
     * @param conn       DBus::Connection object, used to retrieve more
     *                   details about the caller
     * @param owner      uid_t of the owner of this object
     * @return ACL::Ptr
     */
    [[nodiscard]] static ACL::Ptr Create(DBus::Connection::Ptr conn,
                                         const uid_t owner);

    /**
     *  Retrieve the owner (uid) of this object
     *
     * @return const uid_t
     */
    const uid_t GetOwner() const noexcept;

    /**
     *  Transfers the owner of this object to a new user (uid)
     *
     * @param new_owner  uid_t of the new owner
     */
    void TransferOwnership(const uid_t new_owner) noexcept;

    /**
     *  Modify the public access flag
     *
     *  When this flag is enabled, the ACL checks are effectively disabled
     *
     * @param pub_access
     */
    void SetPublicAccess(bool pub_access) noexcept;

    /**
     *  Retrieve the current public access flag setting
     *
     * @return true if anyone has access to this object, otherwise false
     */
    const bool GetPublicAccess() const noexcept;

    /**
     *  Grant a user (uid) access to this object
     *
     * @param uid  uid_t with the user to be granted access
     *
     * @throws ACLException if the uid is already granted access
     */
    void GrantAccess(const uid_t uid);

    /**
     *  Revoke access for a user (uid) from this object
     *
     * @param uid  uid_t with the user to revoke the access from
     *
     * @throws ACLException if the uid has not been granted access
     */
    void RevokeAccess(const uid_t uid);

    /**
     *  Retrieve a list of all users granted access to this object
     *
     * @return ACLList std::vector<uid_t> containing all the uids of users
     *         granted access to the object
     */
    ACLList GetAccessList() const noexcept;

    /**
     *  Validates if a D-Bus caller should be granted access or not
     *
     *  This method will lookup the caller's uid via D-Bus service lookups
     *  and check that uid against the access control list of granted users.
     *
     *  This calls DBus::Credentials::Query::GetUID() to retrieve the callers
     *  uid.
     *
     * @param caller                D-Bus bus name of the calling user
     * @param extra_acl             Optional, ACLList with additional uids to
     *                              give access
     * @param ignore_public_access  Optional, boolean flag (default diabled) to
     *                              do a full ACL check regardless of the public
     *                              access flag
     * @return true if the caller should be granted access, otherwise false
     */
    [[nodiscard]] const bool CheckACL(const std::string &caller,
                                      const ACLList &extra_acl = {},
                                      const bool ignore_public_access = false) const;

    /**
     *  Checks if the D-Bus caller is the owner of this object or not
     *
     *  This calls DBus::Credentials::Query::GetUID() to retrieve the callers
     *  uid.
     *
     * @param caller   D-Bus bus name of the calling user
     * @return true if the calling user's UID matches the owner of this object,
     *          otherwise false
     */
    [[nodiscard]] const bool CheckOwnerAccess(const std::string &caller) const;


  private:
    uid_t owner;
    bool acl_public = false;
    ACLList acl_list{};
    DBus::Credentials::Query::Ptr creds_qry = nullptr;

    ACL(DBus::Connection::Ptr conn, const uid_t owner_);
};


} // namespace GDBusPP::Object::Extension
