//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 *  @file log-internal.hpp
 *
 *  @brief Internal functions used by the net.openvpn.v3.log service
 *         implementation
 */

#include <gdbuspp/object/base.hpp>

#include "log-service.hpp"

namespace LogService::internal {

inline LogMetaData::Ptr prepare_metadata(ServiceHandler *object,
                                         const std::string &method,
                                         DBus::Object::Method::Arguments::Ptr args)
{
    auto meta = LogMetaData::Create();
    meta->AddMeta("sender", args->GetCallerBusName());
    meta->AddMeta("object_path", object->GetPath());
    meta->AddMeta("interface", object->GetInterface());
    meta->AddMeta("method", method);
    return meta;
}

} // namespace LogService::internal
