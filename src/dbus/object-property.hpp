//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2018         Antonio Quartulli <antonio@openvpn.net>
//  Copyright (C) 2018         David Sommerseth <davids@openvpn.net>
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
class PropertyType : public Property
{
public:
    PropertyType(DBusObject *obj_arg, std::string name_arg, std::string
                 dbus_type_arg, std::string dbus_acl_arg, bool allow_root_arg,
                 T * value_arg)
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

    virtual GVariant *GetValue() const override
    {
        return g_variant_new(dbus_type.c_str(), *value);
    }

    virtual GVariantBuilder *SetValue(GVariant *value_arg) override
    {
        g_variant_get(value_arg, dbus_type.c_str(), value);
        return obj->build_set_property_response(name, *value);
    }

    virtual std::string GetName() const
    {
        return name;
    }

private:
    DBusObject *obj;
    std::string name;
    std::string dbus_type;
    std::string dbus_acl;
    bool allow_root;
    T *value = nullptr;
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
