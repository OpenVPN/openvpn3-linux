//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>


namespace DevPosture {

class Module
{
  public:
    using Dictionary = std::unordered_map<std::string, std::string>;
    using UPtr = std::unique_ptr<Module>;


    /**
     *  Create a new Module based object and prepare it for
     *  the D-Bus service.
     *
     * @tparam C      Device Posture Module class to instantiate
     * @tparam T      Class constructor argument types
     * @param args    Class constructor arguments
     *
     * @return Module::UPtr  A unique pointer to the created and instantiated
     *                       device posture checker module
     */
    template <typename C, typename... T>
    [[nodiscard]] static Module::UPtr Create(T &&...args)
    {
        return std::make_unique<C>(C(std::forward<T>(args)...));
    }

    virtual ~Module() = default;

    //
    // Methods
    //

    /**
     *  Runs the device posture check for this module
     *
     * @param input         TODO: TBD.
     * @return Dictionary   A key -> value container holding
     *                      macro -> value_to_substitute pairs.
     *                      Used by the caller to compose JSONs.
     */
    virtual Dictionary Run(const Dictionary &input) = 0;


    //
    // Properties
    //

    /**
     *  Retrieve the name of the device posture check module
     *
     * @return std::string
     */
    virtual std::string name() const = 0;

    /**
     *  Retrieve the device posture check categorization
     *
     * @return std::string
     */
    virtual std::string type() const = 0;

    /**
     *  Retrieve the version number for this device posture check module
     *
     * @return uint16_t
     */
    virtual uint16_t version() const = 0;
};

} // namespace DevPosture
