//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2019  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2019  Lev Stipakov <lev@openvpn.net>
//  Copyright (C) 2019         David Sommerseth <davids@openvpn.net>
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

#include "netcfg-changeevent.hpp"



NetCfgChangeEvent::NetCfgChangeEvent(const NetCfgChangeType& t,
                                     const std::string& dev,
                                     const NetCfgChangeDetails& d) noexcept
{
    reset();
    type = t;
    device = dev;
    details = d;
}


NetCfgChangeEvent::NetCfgChangeEvent(GVariant *params)
{
    std::string g_type(g_variant_get_type_string(params));
    if ("(usa{ss})" != g_type)
    {
        throw NetCfgException(std::string("Invalid GVariant data type: ")
                              + g_type);
    }

    gchar *dev = nullptr;
    GVariantIter *det = nullptr;
    guint tmp_type = 0;
    g_variant_get(params, "(usa{ss})", &tmp_type, &dev, &det);
    type = (NetCfgChangeType) tmp_type;

    device = std::string(dev);
    g_free(dev);
    details.clear();

    GVariant *kv = nullptr;
    while ((kv = g_variant_iter_next_value(det)))
    {
        gchar *key = nullptr;
        gchar *val = nullptr;
        g_variant_get(kv, "{ss}", &key, &val);
        details[key] = std::string(val);
        g_free(key);
        g_free(val);
    }
    g_variant_iter_free(det);
}


NetCfgChangeEvent::NetCfgChangeEvent() noexcept
{
    reset();
}


void NetCfgChangeEvent::reset() noexcept
{
    type = NetCfgChangeType::UNSET;
    device.clear();
    details.clear();
}


bool NetCfgChangeEvent::empty() const noexcept
{
        return (NetCfgChangeType::UNSET == type
                && device.empty() && details.empty());
}


GVariant * NetCfgChangeEvent::GetGVariant() const
{
    GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE("(usa{ss})"));
    g_variant_builder_add(b, "u", (guint32) type);
    g_variant_builder_add(b, "s", device.c_str());

    g_variant_builder_open(b, G_VARIANT_TYPE ("a{ss}"));
    for (const auto& e : details)
    {
        // WARNING: For some odd reason, these four lines
        // below this code context triggers a memory leak
        // warning in valgrind.  This is currently believed
        // to be a false positive, as extracting this code
        // to a separate and minimal program does not trigger
        // this leak warning despite being practically the same
        // code.
        g_variant_builder_open(b, G_VARIANT_TYPE("{ss}"));
        g_variant_builder_add(b, "s", e.first.c_str());
        g_variant_builder_add(b, "s", e.second.c_str());
        g_variant_builder_close(b);
    }
    g_variant_builder_close(b);

    GVariant *ret = g_variant_builder_end(b);
    g_variant_builder_clear(b);
    g_variant_builder_unref(b);
    return ret;
}


bool NetCfgChangeEvent::operator==(const NetCfgChangeEvent& compare) const
{
    return ((compare.type== (const NetCfgChangeType) type)
            && (0 == compare.device.compare(device))
            && (compare.details == details));

}


bool NetCfgChangeEvent::operator!=(const NetCfgChangeEvent& compare) const
{
    return !(this->operator==(compare));
}
