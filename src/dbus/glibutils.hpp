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


#include <vector>
#include <string>
#include <type_traits>

#include "exceptions.hpp"


namespace GLibUtils
{
    /*
     * Method via template specialisation to return the
     *  D-Bus data type for a C type
     */

    // Declare template as prototype only so it cannot be used directly
    template<typename T> inline const char* GetDBusDataType();

    template<> inline const char* GetDBusDataType<uint32_t>()
    {
        return "u";
    }

    /*
     * Since long and int are separate types and int32_t is most times defined as
     * int on ILP32 platform, do we do this template magic to match all signed 32 bit
     * types instead of just int32_t
     */
    template<typename T>
    inline typename std::enable_if<sizeof(T)==4 && std::is_signed<T>::value,const char*>::type
    GetDBusDataType()
    {
        return "i";
    }

    template<> inline const char* GetDBusDataType<uint16_t>()
    {
        return "q";
    }

    template<> inline const char* GetDBusDataType<int16_t>()
    {
        return "n";
    }

    template<> inline const char* GetDBusDataType<uint64_t>()
    {
        return "t";
    }

    template<> inline const char* GetDBusDataType<int64_t>()
    {
        return "x";
    }

    template<> inline const char* GetDBusDataType<double>()
    {
        return "d";
    }

    template<> inline const char* GetDBusDataType<bool>()
    {
        return "b";
    }

    template<> inline const char* GetDBusDataType<std::string>()
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
    template<typename T> inline T GetVariantValue(GVariant *v);

    template<> inline uint32_t GetVariantValue<uint32_t>(GVariant *v)
    {
        return g_variant_get_uint32(v);
    }

    /*
     * Since long and int are separate types and int32_t is most times defined as
     * int on ILP32 platform, do we do this template magic to match all signed 32 bit
     * types instead of just int32_t
     */
    template<typename T>
    inline typename std::enable_if<sizeof(T)==4 && std::is_signed<T>::value,int32_t>::type
    GetVariantValue(GVariant *v)
    {
        return g_variant_get_int32(v);
    }

    template<> inline uint16_t GetVariantValue<uint16_t>(GVariant *v)
    {
        return g_variant_get_uint16(v);
    }

    template<> inline int16_t GetVariantValue<int16_t>(GVariant *v)
    {
        return g_variant_get_int16(v);
    }

    template<> inline uint64_t GetVariantValue<uint64_t>(GVariant *v)
    {
        return g_variant_get_uint64(v);
    }

    template<> inline int64_t GetVariantValue<int64_t>(GVariant *v)
    {
        return g_variant_get_int64(v);
    }

    template<> inline bool GetVariantValue<bool>(GVariant *v)
    {
        return g_variant_get_boolean(v);
    }

    template<> inline std::string GetVariantValue<std::string>(GVariant *v)
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
    template<typename T> inline GVariant* CreateVariantValue(const char* dbustype, T value)
    {
        return g_variant_new(dbustype, value);
    }

    template<> inline GVariant* CreateVariantValue(const char* dbustype, std::string value)
    {
        return g_variant_new(dbustype, value.c_str());
    }

    template<typename T> inline GVariant* CreateVariantValue(T value)
    {
        return g_variant_new(GetDBusDataType<T>(), value);
    }

    template<> inline GVariant* CreateVariantValue(std::string value)
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
    template<typename T> inline void GVariantBuilderAdd(GVariantBuilder *builder, T value)
    {
        g_variant_builder_add(builder, GetDBusDataType<T>(), value);
    }


    /**
     *   Variant of @GVariantBuilderAdd() which tackles C++ std::string
     *
     *   @param builder  GVariantBuilder object where to add the value
     *   @param value    std::string value to add to the GVariantBuilder object
     */
    template<> inline void GVariantBuilderAdd(GVariantBuilder *builder, std::string value)
    {
        g_variant_builder_add(builder, GetDBusDataType<std::string>(), value.c_str());
    }

    /**
     *  Converts a std::vector<T> to a D-Bus compliant
     *  array  builder of the D-Bus corresponding data type
     *
     * @param input  std::vector<T> to convert
     *
     * @return Returns a GVariantBuilder object containing the complete array
     */
    template<typename T> inline
    GVariantBuilder* GVariantBuilderFromVector(const std::vector<T> input)
    {
        std::string type = "a" + std::string(GetDBusDataType<T>());
        GVariantBuilder *bld = g_variant_builder_new(G_VARIANT_TYPE(type.c_str()));
        for (const auto& e : input)
        {
            GVariantBuilderAdd(bld, e);
        }

        return bld;
    }

    /**
     *  Converts a std::vector<T> to a D-Bus compliant
     *  array of the D-Bus corresponding data type
     *
     * @param input  std::vector<T> to convert
     *
     * @return Returns a GVariant object containing the complete array
     */
    template<typename T> inline
    GVariant* GVariantFromVector(const std::vector<T> input)
    {
        GVariantBuilder *bld = GVariantBuilderFromVector(input);
        GVariant *ret = g_variant_builder_end(bld);
        g_variant_builder_unref(bld);

        return ret;
    }

    /**
     * Wraps the list/struct build a builder inside a tuple since DBUS
     * seems to insist on wrapped tuples for fucntions call
     *
     * This function unrefs the builder
     * @param bld the builder to wrap
     * @return the result of the builder wrapped into a tuple
     */
    inline GVariant* wrapInTuple(GVariantBuilder *bld)
    {
        GVariantBuilder *wrapper = g_variant_builder_new(G_VARIANT_TYPE_TUPLE);
        g_variant_builder_add_value(wrapper, g_variant_builder_end(bld));
        GVariant* ret = g_variant_builder_end(wrapper);
        g_variant_builder_unref(wrapper);
        g_variant_builder_unref(bld);
        return ret;
    }

    /**
     *  Converts a std::vector<T> to a D-Bus compliant
     *  array of the D-Bus corresponding data type wrapped into
     *  a tuple
     *
     * @param input  std::vector<T> to convert
     *
     * @return Returns a GVariant object containing the complete array
     */
    template<typename T> inline
    GVariant* GVariantTupleFromVector(const std::vector<T> input)
    {
        GVariantBuilder *bld = GVariantBuilderFromVector(input);
        return wrapInTuple(bld);
    }


    /**
     *  Creates an empty variant object based on more complex types.
     *  The implementation of this method is very basic and simple, without
     *  any type checking, except of what @g_variant_builder_new() provides.
     *
     *  This function is useful in @callback_get_property() methods when
     *  an empty dictionary or array needs to be returned.
     *
     * @param type  String containing the D-Bus data type to return
     * @return Returns a new GVariant based object with the given data type but
     *         without any values.
     */
    inline GVariant* CreateEmptyBuilderFromType(const char* type)
    {
        GVariantBuilder* b = g_variant_builder_new(G_VARIANT_TYPE(type));
        GVariant* ret = g_variant_builder_end(b);
        g_variant_builder_unref(b);
        return ret;
    }


    /**
     *  Validate the data type provided in a GVariant object against
     *  a string containing the expected data type.
     *
     * @param func     C string containing the calling functions name,
     *                 used if an exception is thrown
     * @param params   GVariant* containing the parameters
     * @param format   C string containing the expected data type string
     * @param num      Number of child elements in the GVariant object.
     *                 If 0, element count will not be considered.
     *
     * @throws THROW_DBUSEXCEPTION
     */
    inline void checkParams(const char* func, GVariant* params,
                            const char* format, unsigned int num = 0)
    {
        std::string typestr = std::string(g_variant_get_type_string(params));
        if (format != typestr || (num > 0 && num != g_variant_n_children(params)))
        {
            THROW_DBUSEXCEPTION(func, "Incorrect parameter format: "
                                + typestr + ", expected " + format
                                + "(elements expected: " + std::to_string(num)
                                + " received: "
                                + std::to_string(g_variant_n_children(params)) + ")");
        }
    }


    /*
     * These methods extracts values from a GVariant tuples object,
     * used as an alternative to g_variant_get() which may cause
     * explosions in some situations.  This method seems to work more
     * reliable and with with less surprises.
     */

    // Declare template as prototype only so it cannot be used directly
    template<typename T> inline T ExtractValue(GVariant *v, int elm);

    template<> inline uint64_t ExtractValue<uint64_t>(GVariant *v, int elm)
    {
        return g_variant_get_uint64(g_variant_get_child_value(v, elm));
    }

    template<> inline int64_t ExtractValue<int64_t>(GVariant *v, int elm)
    {
        return g_variant_get_int64(g_variant_get_child_value(v, elm));
    }

    template<> inline uint32_t ExtractValue<uint32_t>(GVariant *v, int elm)
    {
        return g_variant_get_uint32(g_variant_get_child_value(v, elm));
    }

    template<> inline int32_t ExtractValue<int32_t>(GVariant *v, int elm)
    {
        return g_variant_get_int32(g_variant_get_child_value(v, elm));
    }

    template<> inline uint16_t ExtractValue<uint16_t>(GVariant *v, int elm)
    {
        return g_variant_get_uint16(g_variant_get_child_value(v, elm));
    }

    template<> inline int16_t ExtractValue<int16_t>(GVariant *v, int elm)
    {
        return g_variant_get_int16(g_variant_get_child_value(v, elm));
    }

    template<> inline bool ExtractValue<bool>(GVariant *v, int elm)
    {
        return g_variant_get_boolean(g_variant_get_child_value(v, elm));
    }

    template<> inline std::string ExtractValue<std::string>(GVariant *v, int elm)
    {
        return std::string(g_variant_get_string(g_variant_get_child_value(v, elm), 0));
    }


    /**
     * Unreferences an fd list. This is a helper function since the normal
     * g_unref_object does not fit the signature and there seem to be no
     * function to properly unref fdlists. The other examples that I found use
     * g_object_unref on the list
     * @param the fd list
     */
    inline void unref_fdlist(GUnixFDList *fdlist)
    {
        g_object_unref((GVariant*) fdlist);
    }

    inline int get_fd_from_invocation(GDBusMethodInvocation *invoc)
    {
        GDBusMessage *dmsg = g_dbus_method_invocation_get_message(invoc);
        GUnixFDList *fdlist = g_dbus_message_get_unix_fd_list(dmsg);

        // Get the first FD from the fdlist list
        int fd = -1;

        if(fdlist != NULL)
        {
            GError *error = nullptr;
            if (fdlist)
            {
                fd = g_unix_fd_list_get(fdlist, 0, &error);
            }

            if (!fdlist || error || fd == -1)
            {
                THROW_DBUSEXCEPTION("GLibUtils", "Reading fd socket failed");
            }
        }

        return fd;
    }

} // namespace GLibUtils
