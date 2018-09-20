//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2018         Antonio Quartulli <antonio@openvpn.net>
//  Copyright (C) 2018         David Sommerseth <davids@openvpn.net>
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


#pragma once

#include <openvpn/common/rc.hpp>

#include "dbus/object.hpp"

using namespace openvpn;

class Property : public RC<thread_safe_refcount>
{
public:
    typedef RCPtr<Property> Ptr;

    virtual std::string GetIntrospectionXML() const = 0;
    virtual bool GetRootAllowed() const = 0;
    virtual GVariant *GetValue() const = 0;
    virtual GVariantBuilder *SetValue(GVariant *value_arg) = 0;
    virtual std::string GetName() const = 0;
};


template <typename T>
class PropertyTypeBase : public Property
{
public:
    PropertyTypeBase(DBusObject *obj_arg, std::string name_arg, std::string
                 dbus_type_arg, std::string dbus_acl_arg, bool allow_root_arg,
                 T & value_arg)
        : obj(obj_arg),
          name(name_arg),
          dbus_type(dbus_type_arg),
          dbus_acl(dbus_acl_arg),
          allow_root(allow_root_arg),
          value(value_arg)
    {
    }


    virtual std::string GetIntrospectionXML() const override
    {
        return "<property type='" + dbus_type + "' name='" + name + "' access='"
               + dbus_acl + "' />";
    }

    virtual bool GetRootAllowed() const override
    {
        return allow_root;
    }


    virtual std::string GetName() const override
    {
        return name;
    }
protected:
    DBusObject *obj;
    std::string name;
    std::string dbus_type;
    std::string dbus_acl;
    bool allow_root;
    T& value;
};


template <typename T>
class PropertyType : public PropertyTypeBase<T>
{
public:
    PropertyType(DBusObject *obj_arg, std::string name_arg, std::string
    dbus_type_arg, std::string dbus_acl_arg, bool allow_root_arg,
                     T& value_arg) :
                     PropertyTypeBase<T>(obj_arg, name_arg, dbus_type_arg,
                         dbus_acl_arg, allow_root_arg, value_arg)
    {

    }

    virtual GVariant *GetValue() const override
    {
        return g_variant_new(PropertyTypeBase<T>::dbus_type.c_str(), PropertyTypeBase<T>::value);
    }

    virtual GVariantBuilder *SetValue(GVariant *value_arg) override
    {
        g_variant_get(value_arg, PropertyTypeBase<T>::dbus_type.c_str(), &(PropertyTypeBase<T>::value));
        return PropertyTypeBase<T>::obj->build_set_property_response(PropertyTypeBase<T>::name, PropertyTypeBase<T>::value);
    }
};

/* Specialised class to handle vectors */

/* These overloaded getVariant with multiple int and std;:string types are here to
 * allow the vector template to be as generic as possible while still calling the right dbus
* function */

// Declare template as prototype only so it cannot be used directly
template<typename T> T getVariantValue(GVariant *v);

template<> uint32_t getVariantValue<uint32_t>(GVariant *v) { return g_variant_get_uint32(v); }
template<> int32_t getVariantValue<int32_t>(GVariant *v) { return g_variant_get_int32(v); }
template<> uint16_t getVariantValue<uint16_t>(GVariant *v) { return g_variant_get_uint16(v); }
template<> int16_t getVariantValue<int16_t>(GVariant *v) { return g_variant_get_int16(v); }
template<> uint64_t getVariantValue<uint64_t>(GVariant *v) { return g_variant_get_uint64(v); }
template<> int64_t getVariantValue<int64_t>(GVariant *v) { return g_variant_get_int64(v); }
template<> std::string getVariantValue<std::string>(GVariant *v) { gsize size=0; return std::string(g_variant_get_string(v, &size)); }

template<unsigned int> unsigned int getVariantValue(GVariant *v) { return g_variant_get_uint32(v); }


template <typename T>
class PropertyType<std::vector<T>> : public PropertyTypeBase<std::vector<T>>
{
public:
    PropertyType<std::vector<T>>(DBusObject *obj_arg,
                                 std::string name_arg,
                                 std::string dbus_array_member_type,
                                 std::string dbus_acl_arg,
                                 bool allow_root_arg,
                                 std::vector<T> &value_arg)
        : PropertyTypeBase<std::vector<T>>(obj_arg, name_arg,
                                           "a" + dbus_array_member_type,
                                           dbus_acl_arg,
                                           allow_root_arg,
                                           value_arg),
                                           dbus_array_member_type(dbus_array_member_type),
                                           dbus_array_type("a" + dbus_array_member_type)
            {
            }


    /**
     *  Returns a GVariant object with this object's vector contents
     *
     * @return GVariant object popluated with this objects vector contents
     */
    virtual GVariant *GetValue() const override
    {
        GVariantBuilder* bld = get_builder();
        GVariant *ret =g_variant_builder_end(bld);
        g_variant_builder_unref(bld);
        return ret;
    }


    /**
     *  Parses a GVariant array and populates this object's std::Vector with
     *  the provided data
     *
     * @param value_arg GVariant
     * @return
     */
    virtual GVariantBuilder* SetValue(GVariant *value_arg) override
    {
        GVariantIter* list;
        g_variant_get(value_arg, dbus_array_type.c_str(), &list);

        GVariant *iter = NULL;
        std::vector<T> newvalue;
        while ((iter = g_variant_iter_next_value(list)))
        {
            newvalue.push_back(getVariantValue<T>(iter));
            g_variant_unref(iter);
        }
        g_variant_unref(value_arg);
        g_variant_iter_free(list);
        this->value = newvalue;
        return get_builder();
    }


private:
    std::string dbus_array_member_type;
    std::string dbus_array_type;

    /**
     *  Creates a proper GVariantBuilder of the corresponding D-Bus
     *  array data type for this C++ std::vector data type.
     *
     * @return  Returns a GVariantBuilder object, populated with the data
     *          of this objects data.
     */
    virtual GVariantBuilder *get_builder() const
    {
        GVariantBuilder *bld = g_variant_builder_new(G_VARIANT_TYPE(dbus_array_type.c_str()));
        for (const auto &e : this->value)
        {
            g_variant_builder_add(bld, dbus_array_member_type.c_str(), e);
        }
        return bld;
    }
};



class PropertyCollection
{
  public:
    PropertyCollection(DBusObject *obj)
    {
    }

    void AddBinding(Property::Ptr prop)
    {
        properties.insert(std::pair<std::string, Property::Ptr>(prop->GetName(), prop));
    }

    bool Exists(std::string name)
    {
        auto prop = properties.find(name);
        return prop != properties.end();
    }

    std::string GetIntrospectionXML()
    {
        std::string xml = "";
        for (auto &prop : properties)
            xml += prop.second->GetIntrospectionXML();

        return xml;
    }

    bool GetRootAllowed(std::string property_name)
    {
        auto prop = properties.find(property_name);
        if (prop == properties.end())
            return false;

        return prop->second->GetRootAllowed();
    }

    GVariant *GetValue(std::string property_name)
    {
        auto prop = properties.find(property_name);
        if (prop == properties.end())
            return NULL;

        return prop->second->GetValue();
    }

    GVariantBuilder *SetValue(std::string property_name,
                                            GVariant *value)
    {
        auto prop = properties.find(property_name);
        if (prop == properties.end())
            return NULL;

        return prop->second->SetValue(value);
    }

  private:
    std::map<std::string, Property::Ptr> properties;
};
