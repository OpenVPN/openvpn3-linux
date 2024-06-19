//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

#include <dbus/constants.hpp>

#include "constants.hpp"
#include "modulehandler.hpp"
#include "sysinfo/sysinfo.hpp"


namespace DevPosture {

ModuleHandler::ModuleHandler(Module::UPtr mod, const bool external)
    : DBus::Object::Base(PATH_DEVPOSTURE + "/modules/" + mod->name(),
                         Constants::GenInterface("devicecheck")),
      module_(std::move(mod)),
      prop_name_(module_->name()),
      prop_type_(module_->type()),
      prop_version_(module_->version()),
      prop_external_(external)

{
    auto r_args = AddMethod(
        "Run",
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            GVariantBuilder *b = glib2::Builder::Create("a{sv}");

            for (const auto &[key, value] : module_->Run({}))
            {
                g_variant_builder_add(b, "{sv}", key.c_str(), glib2::Value::Create(value));
            }

            args->SetMethodReturn(glib2::Builder::FinishWrapped(b));
        });

    r_args->AddInput("input", "a{sv}");
    r_args->AddOutput("result", "a{sv}");

    AddProperty("type", prop_type_, /* readwrite */ false);
    AddProperty("name", prop_name_, /* readwrite */ false);
    AddProperty("version", prop_version_, /* readwrite */ false);
    AddProperty("external", prop_external_, /* readwrite */ false);
}


const bool ModuleHandler::Authorize(const DBus::Authz::Request::Ptr authzreq)
{
    return true;
}

} // namespace DevPosture
