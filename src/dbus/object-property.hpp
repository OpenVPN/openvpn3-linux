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
#include "glibutils.hpp"

using namespace openvpn;

class Property : public RC<thread_safe_refcount>
{
public:
    typedef RCPtr<Property> Ptr;

    virtual std::string GetIntrospectionXML() const = 0;
    virtual bool GetManagerAllowed() const = 0;
    virtual GVariant *GetValue() const = 0;
    virtual GVariantBuilder *SetValue(GVariant *value_arg) = 0;
    virtual std::string GetName() const = 0;
};



template <typename T>
class PropertyTypeBase : public Property
{
public:
    PropertyTypeBase(DBusObject *obj_arg,
                     const std::string & name_arg,
                     const std::string & dbus_acl_arg,
                     bool allow_mngr_arg,
                     T & value_arg)
        : obj(obj_arg),
          name(name_arg),
          dbus_acl(dbus_acl_arg),
          allow_manager(allow_mngr_arg),
          value(value_arg)
    {
    }


    virtual std::string GetIntrospectionXML() const override
    {
        return "<property type='" + std::string(GetDBusType()) + "' "
               + " name='" + name + "' access='" + dbus_acl + "' />";
    }


    virtual bool GetManagerAllowed() const override
    {
        return allow_manager;
    }


    virtual const char* GetDBusType() const=0;


    virtual std::string GetName() const override
    {
        return name;
    }


protected:
    DBusObject *obj;
    std::string name;
    std::string dbus_acl;
    bool allow_manager;
    T& value;
};



template <typename T>
class PropertyType : public PropertyTypeBase<T>
{
public:
    PropertyType(DBusObject *obj_arg, std::string name_arg,
                 std::string dbus_acl_arg, bool allow_mngr_arg,
                 T& value_arg, std::string override_dbus_type = "")
        : PropertyTypeBase<T>(obj_arg, name_arg,
                              dbus_acl_arg, allow_mngr_arg,
                              value_arg),
          override_dbus_type(override_dbus_type)
    {

    }


    virtual const char* GetDBusType() const override
    {
        return (override_dbus_type.empty()
                ? GLibUtils::GetDBusDataType<T>()
                : override_dbus_type.c_str());
    }


    virtual GVariant *GetValue() const override
    {
        return GLibUtils::CreateVariantValue(GetDBusType(), PropertyTypeBase<T>::value);
    }

    virtual GVariantBuilder *SetValue(GVariant *value_arg) override
    {
        PropertyTypeBase<T>::value = GLibUtils::GetVariantValue<T>(value_arg);
        return PropertyTypeBase<T>::obj->build_set_property_response(PropertyTypeBase<T>::name, PropertyTypeBase<T>::value);
    }

private:
    const std::string override_dbus_type;
};


/**
 *   Specialised class to handle property values based on vectors
 */
template <typename T>
class PropertyType<std::vector<T>> : public PropertyTypeBase<std::vector<T>>
{
public:
    PropertyType<std::vector<T>>(DBusObject *obj_arg,
                                 std::string name_arg,
                                 std::string dbus_acl_arg,
                                 bool allow_mngr_arg,
                                 std::vector<T> &value_arg)
        : PropertyTypeBase<std::vector<T>>(obj_arg, name_arg,
                                           dbus_acl_arg,
                                           allow_mngr_arg,
                                           value_arg),
                                           dbus_array_type("a" + std::string(GLibUtils::GetDBusDataType<T>()))
    {
    }


    /**
     *  Retrieve the D-Bus data type of this property
     *
     * @return  Returns a C char based string containing the GVariant
     *          compatible data type in use.
     */
    virtual const char* GetDBusType() const override
    {
        return dbus_array_type.c_str();
    }


    /**
     *  Returns a GVariant object with this object's vector contents
     *
     * @return GVariant object popluated with this objects vector contents
     */
    virtual GVariant *GetValue() const override
    {
        GVariantBuilder* bld = get_builder();
        GVariant *ret = g_variant_builder_end(bld);
        g_variant_builder_unref(bld);
        return ret;
    }


    /**
     *  Parses a GVariant array and populates this object's std::vector with
     *  the provided data
     *
     * @param value_arg  GVariant object containing the array to decode into
     *                   a C++ based std::vector/array.
     *
     * @return  Returns a GVariantBuilder with the vector/array of the input
     *          data.  This will be used when sending the PropertyChanged
     *          D-Bus signal.
     */
    virtual GVariantBuilder* SetValue(GVariant *value_arg) override
    {
        GVariantIter* list;
        g_variant_get(value_arg, dbus_array_type.c_str(), &list);

        GVariant *iter = NULL;
        std::vector<T> newvalue;
        while ((iter = g_variant_iter_next_value(list)))
        {
            newvalue.push_back(GLibUtils::GetVariantValue<T>(iter));
            g_variant_unref(iter);
        }
        g_variant_unref(value_arg);
        g_variant_iter_free(list);
        this->value = newvalue;
        return get_builder();
    }


private:
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
            GLibUtils::GVariantBuilderAdd(bld, e);
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

    bool GetManagerAllowed(std::string property_name)
    {
        auto prop = properties.find(property_name);
        if (prop == properties.end())
            return false;

        return prop->second->GetManagerAllowed();
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
