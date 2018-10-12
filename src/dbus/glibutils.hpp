//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018         Arne Schwabe <arne@openvpn.net>
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
 * @file   glibutils.hpp
 *
 * @brief  Various utility functions bridging the gap between GLib2 C
 *         and modern C++
 */

#pragma once

namespace GLibUtils
{
    /*
     * Method via template specialisation to return the
     *  D-Bus data type for a C type
     */

    // Declare template as prototype only so it cannot be used directly
    template<typename T> const char* GetDBusDataType();

    template<> const char* GetDBusDataType<uint32_t>()
    {
        return "u";
    }

    template<> const char* GetDBusDataType<int32_t>()
    {
        return "i";
    }

    template<> const char* GetDBusDataType<uint16_t>()
    {
        return "q";
    }

    template<> const char* GetDBusDataType<int16_t>()
    {
        return "n";
    }

    template<> const char* GetDBusDataType<uint64_t>()
    {
        return "t";
    }

    template<> const char* GetDBusDataType<int64_t>()
    {
        return "x";
    }

    template<> const char* GetDBusDataType<double>()
    {
        return "d";
    }

    template<> const char* GetDBusDataType<bool>()
    {
        return "b";
    }

    template<> const char* GetDBusDataType<std::string>()
    {
        return "s";
    }

    /*
     * These overloaded GetVariantValue with multiple int and std::string
     * types are here to allow the vector template to be as generic as
     * possible while still calling the right D-Bus function
     *
     */

    // Declare template as prototype only so it cannot be used directly
    template<typename T> T GetVariantValue(GVariant *v);

    template<> uint32_t GetVariantValue<uint32_t>(GVariant *v)
    {
        return g_variant_get_uint32(v);
    }

    template<> int32_t GetVariantValue<int32_t>(GVariant *v)
    {
        return g_variant_get_int32(v);
    }

    template<> uint16_t GetVariantValue<uint16_t>(GVariant *v)
    {
        return g_variant_get_uint16(v);
    }

    template<> int16_t GetVariantValue<int16_t>(GVariant *v)
    {
        return g_variant_get_int16(v);
    }

    template<> uint64_t GetVariantValue<uint64_t>(GVariant *v)
    {
        return g_variant_get_uint64(v);
    }

    template<> int64_t GetVariantValue<int64_t>(GVariant *v)
    {
        return g_variant_get_int64(v);
    }

    template<> bool GetVariantValue<bool>(GVariant *v)
    {
        return g_variant_get_boolean(v);
    }

    template<> std::string GetVariantValue<std::string>(GVariant *v)
    {
        gsize size = 0;
        return std::string(g_variant_get_string(v, &size));
    }

    /**
     *   Template variant of GLib2's @g_variant_builder_add() which extract
     *   the D-Bus data type automatically via the data type passed.
     *
     *   @param dbustype DBus name auf the type
     *   @param value    Templated value to add to the GVariantBuilder object
     */
    template<typename T> GVariant* CreateVariantValue(const char* dbustype, T value)
    {
        return g_variant_new(dbustype, value);
    }

    template<> GVariant* CreateVariantValue(const char* dbustype, std::string value)
    {
        return g_variant_new(dbustype, value.c_str());
    }

    template<typename T> GVariant* CreateVariantValue(T value)
    {
        return g_variant_new(GetDBusDataType<T>(), value);
    }

    template<> GVariant* CreateVariantValue(std::string value)
    {
        return g_variant_new(GetDBusDataType<std::string>(), value.c_str());
    }

    /**
     *   Template variant of GLib2's @g_variant_builder_add() which extract
     *   the D-Bus data type automatically via the data type passed.
     *
     *   @param builder  GVariantBuilder object where to add the value
     *   @param value    Templated value to add to the GVariantBuilder object
     */
    template<typename T> void GVariantBuilderAdd(GVariantBuilder *builder, T value)
    {
        g_variant_builder_add(builder, GetDBusDataType<T>(), value);
    }


    /**
     *   Variant of @GVariantBuilderAdd() which tackles C++ std::string
     *
     *   @param builder  GVariantBuilder object where to add the value
     *   @param value    std::string value to add to the GVariantBuilder object
     */
    template<> void GVariantBuilderAdd(GVariantBuilder *builder, std::string value)
    {
        g_variant_builder_add(builder, GetDBusDataType<std::string>(), value.c_str());
    }


    /**
     *  Converts a std::vector<T> to a D-Bus compliant
     *  array of the D-Bus corresponding data type
     *
     * @param input  std::vector<T> to convert
     *
     * @return Returns a GVariant object containing the complete array
     */
    template<typename T>
    GVariant * GVariantFromVector(const std::vector<T> input)
    {
        std::string type = "a" + std::string(GetDBusDataType<T>());
        GVariantBuilder *bld = g_variant_builder_new(G_VARIANT_TYPE(type.c_str()));
        for (const auto& e : input)
        {
            GVariantBuilderAdd(bld, e);
        }

        GVariant *ret = g_variant_builder_end(bld);
        g_variant_builder_unref(bld);

        return ret;
    }
} // namespace GLibUtils
