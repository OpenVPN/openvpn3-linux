//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

#include <gdbuspp/service.hpp>
#include <gdbuspp/connection.hpp>

#include "modules/module-interface.hpp"


namespace DevPosture {

/**
 *  The D-Bus handler object for a specific device posture check module
 */
class ModuleHandler : public DBus::Object::Base
{
  public:
    using Ptr = std::shared_ptr<ModuleHandler>;

  public:
    /**
     *  Instantiates a new device posture module and sets up the D-Bus
     *  object accordingly
     *
     * @param mod       Module::UPtr (aka std::unique_ptr<Module>) to the
     *                  device posture check module to register in the
     *                  service
     * @param external  bool flag, is this an externally (dynamically) loaded
     *                  device posture module (true), or an internal module?
     */
    ModuleHandler(Module::UPtr mod, const bool external);

    const bool Authorize(const DBus::Authz::Request::Ptr authzreq);

    /**
     *  Run the device posture check
     *
     * @param input                TODO: TBD.
     * @return Module::Dictionary  a key -> value container, where the key is
     *                             basically the name of a "macro" to be
     *                             substituted by the caller with the contents
     *                             of the value. The values contain the actual
     *                             information extracted by the Module from the
     *                             system.
     */
    Module::Dictionary Run(const Module::Dictionary &input) const
    {
        // PIMPL-ish.
        return module_->Run(input);
    }

  private:
    const Module::UPtr module_ = nullptr;  ///< Device check module
    std::string prop_name_ = "";           ///< Name of the device posture check
    std::string prop_type_ = "";           ///< Device posture check categorization
    uint16_t prop_version_ = 0;            ///< Version of the device posture check
    bool prop_external_ = false;           ///< Indicates externally loaded check
};

} // namespace DevPosture
