//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

#include <iostream>
#include "../../../src/client/proxy-devposture.hpp"

int main()
{
    try
    {
        auto connection = DBus::Connection::Create(DBus::BusType::SYSTEM);
        auto proxy = DevPosture::Proxy::Handler::Create(connection);

        std::cout << "*** Registered modules:\n";

        for (const auto &m : proxy->GetRegisteredModules())
        {
            std::cout << m << "\n";
        }

        const std::string DPC_REQUEST = R"({
            "dpc_request": {
                "ver": "1.0",
                "correlation_id": "3fa85f64-5717-4562-b3fc-2c963f66afa6",
                "timestamp": "Wed May 29 22:46:01.541 2024",
                "client_info": true
            }
        })";

        std::cout << "*** ProtocolLookup(\"openvpninc\"): "
                  << proxy->ProtocolLookup("openvpninc")
                  << "\n*** RunChecks(\"dpc1\", DPC_REQUEST): "
                  << proxy->RunChecks("dpc1", DPC_REQUEST) << "\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception caught: " << e.what() << "\n";
        return 1;
    }
}
