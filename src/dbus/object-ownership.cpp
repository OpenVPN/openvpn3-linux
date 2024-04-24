//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017 -  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017 -  David Sommerseth <davids@openvpn.net>
//

/**
 * @file object-ownership.hpp
 *
 * @brief Implementation of the DBus::Object::Base ownership and ACL management
 */

#include <algorithm>

#include "object-ownership.hpp"


namespace GDBusPP::Object::Extension {


ACLException::ACLException(const std::string &msg)
    : DBus::Object::Exception(msg, nullptr)
{
}


ACL::Ptr ACL::Create(DBus::Connection::Ptr conn, const uid_t owner)
{
    return ACL::Ptr(new ACL(conn, owner));
}


const uid_t ACL::GetOwner() const noexcept
{
    return owner;
}

void ACL::TransferOwnership(const uid_t new_owner) noexcept
{
    owner = new_owner;
}

void ACL::SetPublicAccess(bool pub_access) noexcept
{
    acl_public = pub_access;
}

const bool ACL::GetPublicAccess() const noexcept
{
    return acl_public;
}


void ACL::GrantAccess(const uid_t uid)
{
    auto fnd = std::find(acl_list.begin(), acl_list.end(), uid);
    if (fnd != std::end(acl_list))
    {
        throw ACLException("UID already granted access");
    }
    acl_list.push_back(uid);
}


void ACL::RevokeAccess(const uid_t uid)
{
    auto fnd = std::find(acl_list.begin(), acl_list.end(), uid);
    if (fnd == std::end(acl_list))
    {
        throw ACLException("UID not granted access");
    }
    acl_list.erase(std::remove(acl_list.begin(), acl_list.end(), uid));
}


ACLList ACL::GetAccessList() const noexcept
{
    return acl_list;
}


const bool ACL::CheckACL(const std::string &caller,
                         const ACLList &extra_acl,
                         const bool ignore_public_access) const
{
    if (!ignore_public_access && acl_public)
    {
        // Everyone is granted access
        return true;
    }

    uid_t caller_uid = creds_qry->GetUID(caller);

    ACLList acl_check = acl_list;
    acl_check.insert(acl_check.end(), extra_acl.begin(), extra_acl.end());
    for (const auto& acl_uid : acl_check)
    {
        if (acl_uid == caller_uid)
        {
            return true;
        }
    }
    return false;
}


const bool ACL::CheckOwnerAccess(const std::string &caller) const
{
    return creds_qry->GetUID(caller) == owner;
}


ACL::ACL(DBus::Connection::Ptr conn, const uid_t owner_)
    : owner(owner_)
{
    creds_qry = DBus::Credentials::Query::Create(conn);
}

} // namespace GDBusExt::Object
