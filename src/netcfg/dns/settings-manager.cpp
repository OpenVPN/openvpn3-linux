//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2019 - 2020  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2020  David Sommerseth <davids@openvpn.net>
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
 * @file   settings-manager.cpp
 *
 *
 * @brief  Main manager object for all DNS resolver settings (implementation)
 */

#include <openvpn/common/rc.hpp>
using namespace openvpn;

#include "dbus/core.hpp"
#include "netcfg/netcfg-exception.hpp"
#include "netcfg/dns/resolver-backend-interface.hpp"
#include "netcfg/dns/settings-manager.hpp"

namespace NetCfg {
namespace DNS {

//
// NetCfg::DNS::SettingsManager
//

SettingsManager::SettingsManager(ResolverBackendInterface::Ptr be)
                  : backend(be)
{
}

SettingsManager::~SettingsManager() = default;


const std::string SettingsManager::GetBackendInfo() const
{
    return backend->GetBackendInfo();
}


ResolverSettings::Ptr SettingsManager::NewResolverSettings()
{
    ResolverSettings::Ptr settings = new ResolverSettings(++resolver_idx);
    resolvers[resolver_idx] = settings;
    return settings;
}


void SettingsManager::RemoveResolverSettings(ResolverSettings::Ptr settings)
{
    if (!settings)
    {
        throw NetCfgException("RemoveResolverSettings(): Invalid ResolverSettings");
    }

    ssize_t idx = settings->GetIndex();
    if (idx < 0)
    {
        throw NetCfgException(
                        "RemoveResolverSettings: Invalid settings");
    }
    resolvers.erase(idx);
}


void SettingsManager::ApplySettings()
{
    for (auto rslv = resolvers.rbegin(); rslv != resolvers.rend(); rslv++)
    {
        if (rslv->second->ChangesAvailable())
        {
            backend->Apply(rslv->second);
        }
    }
    backend->Commit();
}


std::vector<std::string> SettingsManager::GetDNSservers() const
{
    std::vector<std::string> ret;
    for (auto rslv = resolvers.rbegin(); rslv != resolvers.rend(); rslv++)
    {
        std::vector<std::string> s = rslv->second->GetNameServers();
        ret.insert(ret.end(), s.begin(), s.end());
    }
    return ret;
}


std::vector<std::string> SettingsManager::GetSearchDomains() const
{
    std::vector<std::string> ret;
    for (auto rslv = resolvers.rbegin(); rslv != resolvers.rend(); rslv++)
    {
        std::vector<std::string> s = rslv->second->GetSearchDomains();
        ret.insert(ret.end(), s.begin(), s.end());
    }
    return ret;
}


} // namespace DNS
} // namespace NetCfg
