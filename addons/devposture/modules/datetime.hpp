//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

#pragma once

#include "module-interface.hpp"


namespace DevPosture {

class DateTimeModule : public Module
{
  public:
    // Methods
    Dictionary Run(const Dictionary &input) override;

    // Properties
    std::string name() const override
    {
        return "datetime";
    }

    std::string type() const override
    {
        return "hostinfo";
    }

    uint16_t version() const override
    {
        return 1;
    }
};

} // namespace DevPosture
