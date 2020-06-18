//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
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

/**
 * @file   service.hpp
 *
 * @brief  D-Bus service for log management
 */

#include <map>
#include <string>
#include <functional>
#include <json/json.h>

#include <openvpn/common/rc.hpp>

#include "dbus/constants.hpp"
#include "dbus/core.hpp"
#include "dbus/connection-creds.hpp"
#include "dbus/path.hpp"
#include "logger.hpp"

using namespace openvpn;

/**
 *  This provides a more generic interface to generate and process
 *  the log tags and hashes used to separate log events from various
 *  attached log senders.
 */
struct LogTag : public RC<thread_unsafe_refcount>
{
    /**
     *  LogTag contstructor
     *
     * @param sender     std::string of the D-Bus unique bus name (1:xxxx)
     * @param interface  std::string of the D-Bus interface sending events
     *
     */
    LogTag(std::string sender, std::string interface)
    {
        tag = std::string("[") + sender + "/" + interface + "]";

        // Create a hash of the tag, used as an index
        std::hash<std::string> hashfunc;
        hash = hashfunc(tag);
    }


    /**
     *  Return a std::string containing the tag to be used with log lines
     *
     *  The structure is: {tag:xxxxxxxxxxx}
     *  where xxxxxxxxxxx is a positive number
     *
     * @return  Returns a std::string containing the tag this sender and
     *           interface will use
     */
    std::string str() const
    {
        return std::string("{tag:") + std::string(std::to_string(hash))
               + std::string("}");
    }


    /**
     *  Write a formatted tag string via iostreams
     *
     * @param os  std::ostream where to write the data
     * @param ev  LogEvent to write to the stream
     *
     * @return  Returns the provided std::ostream together with the
     *          decoded LogEvent information
     */
    friend std::ostream& operator<<(std::ostream& os , const LogTag& ltag)
    {
        return os << "{tag:" << std::to_string(ltag.hash) << "}";
    }

    std::string tag;  /**<  Contains the string used for the hash generation */
    size_t hash;      /**<  Contains the hash value for this LogTag */
};

/**
 *  The LogServiceManager maintains the D-Bus object to be used
 *  when attaching, detaching and otherwise manage the log processing
 *  over D-Bus
 */
class LogServiceManager : public DBusObject,
                          public DBusConnectionCreds,
                          public RC<thread_unsafe_refcount>
{
public:
    typedef RCPtr<LogServiceManager> Ptr;

    /**
     *  Initializes the LogServiceManager object.
     *
     * @param dbcon    GDBusConnection pointer to use for this service.
     * @param objpath  String with object path this manager should be
     *                 with registered.
     * @param logwr    Pointer to a LogWriter object which handles log writes
     * @param log_level Unsigned int with initial default log level to use
     *
     */
    LogServiceManager(GDBusConnection *dbcon, const std::string objpath,
                      LogWriter *logwr, const unsigned int log_level)
                    : DBusObject(objpath),
                      DBusConnectionCreds(dbcon),
                      dbuscon(dbcon),
                      logwr(logwr),
                      log_level(log_level),
                      statedir("")
    {
        // Restrict extended access in this log service from these
        // well-known bus names primarily.
        //
        // When the backend VPN client process attaches to the log service
        // it will be granted management access to its own log subscription,
        // but this is checked later.
        //
        allow_list.push_back(OpenVPN3DBus_name_backends);
        allow_list.push_back(OpenVPN3DBus_name_sessions);
        allow_list.push_back(OpenVPN3DBus_name_configuration);

        std::stringstream introspection_xml;
        introspection_xml << "<node name='" << objpath << "'>"
        << "    <interface name='" << OpenVPN3DBus_interf_log << "'>"
        << "        <method name='Attach'>"
        << "            <arg type='s' name='interface' direction='in'/>"
        << "        </method>"
        << "        <method name='Detach'>"
        << "            <arg type='s' name='interface' direction='in'/>"
        << "        </method>"
        << "        <method name='GetSubscriberList'>"
        << "            <arg type='a(ssss)' name='subscribers' direction='out'/>"
        << "        </method>"
        << "        <property type='s' name='version' access='read'/>"
        << "        <property name='log_level' type='u' access='readwrite'/>"
        << "        <property name='log_dbus_details' type='b' access='readwrite'/>"
        << "        <property name='timestamp' type='b' access='readwrite'/>"
        << "        <property name='num_attached' type='u' access='read'/>"
        << "    </interface>"
        << "</node>";
        ParseIntrospectionXML(introspection_xml);
    }

    ~LogServiceManager() = default;


    /**
     *  If called, the LogServiceManager will save each settings change to
     *  a state file in this directory.  When called it will attempt to
     *  load a possibly previous saved state file.
     *
     * @param sd  std::string containing the directory where to save the state
     */
    void SetStateDirectory(std::string sd)
    {
        if (sd.empty())
        {
            // Ignore this
            return;
        }
        statedir = sd;
        load_state();
    }

    /**
     *  Callback method which is called each time a D-Bus method call occurs
     *  on this LogServiceManager object.
     *
     * @param conn        D-Bus connection where the method call occurred.
     * @param sender      D-Bus bus name of the sender of the method call.
     * @param obj_path    D-Bus object path of the target object.
     * @param intf_name   D-Bus interface of the method call.
     * @param method_name D-Bus method name to be executed.
     * @param params      GVariant Glib2 object containing the arguments for
     *                    the method call.
     * @param invoc       GDBusMethodInvocation where the response/result of
     *                    the method call will be returned.
     */
    virtual void callback_method_call(GDBusConnection *conn,
                                      const std::string sender,
                                      const std::string obj_path,
                                      const std::string intf_name,
                                      const std::string meth_name,
                                      GVariant *params,
                                      GDBusMethodInvocation *invoc)
    {
        std::stringstream meta;
        meta << "sender=" << sender
             << ", object_path=" << obj_path
             << ", interface=" << intf_name
             << ", method=" << meth_name;

        try
        {
            IdleCheck_UpdateTimestamp();

            // Extract the interface to operate on.  All D-Bus method
            // calls expects this information.
            std::string interface;
            if ("GetSubscriberList" != meth_name)
            {
                GLibUtils::checkParams(__func__, params, "(s)", 1);
                interface = GLibUtils::ExtractValue<std::string>(params, 0);
            }

            if ("Attach" == meth_name)
            {
                LogTag tag(sender, interface);

                // Subscribe to signals from a new D-Bus service/client

                // Check this has not been already registered
                if (loggers.find(tag.hash) != loggers.end())
                {
                    std::stringstream l;
                    l << "Duplicate: " << tag << "  " << tag.tag;

                    logwr->AddMeta(meta.str());
                    logwr->Write(LogEvent(LogGroup::LOGGER, LogCategory::WARN,
                                          l.str()));

                    GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.log",
                                                                  "Already registered");
                    g_dbus_method_invocation_return_gerror(invoc, err);
                    g_error_free(err);
                    return;
                }

                loggers[tag.hash].reset(new Logger(dbuscon, logwr, tag.str(),
                                              sender, interface, log_level));

                std::stringstream l;
                l << "Attached: " << tag << "  " << tag.tag;

                logwr->AddMeta(meta.str());
                logwr->Write(LogEvent(LogGroup::LOGGER, LogCategory::VERB2,
                                      l.str()));
                IdleCheck_RefInc();

                g_dbus_method_invocation_return_value(invoc, NULL);
            }
            else if ("Detach" == meth_name)
            {
                LogTag tag(sender, interface);

                // Ensure the requested logger is truly configured
                if (loggers.find(tag.hash) == loggers.end())
                {
                    std::stringstream l;
                    l << "Not found: " << tag << " " << tag.tag;

                    logwr->AddMeta(meta.str());
                    logwr->Write(LogEvent(LogGroup::LOGGER, LogCategory::WARN,
                                          l.str()));

                    GError *err = g_dbus_error_new_for_dbus_error(
                                    "net.openvpn.v3.error.log",
                                    "Log registration not found");
                    g_dbus_method_invocation_return_gerror(invoc, err);
                    g_error_free(err);
                    return;
                }

                // Check this has not been already registered
                validate_sender(sender, loggers[tag.hash]->GetBusName());

                // Unsubscribe from signals from a D-Bus service/client
                loggers.erase(tag.hash);
                std::stringstream l;
                l << "Detached: " << tag << "  " << tag.tag;

                logwr->AddMeta(meta.str());
                logwr->Write(LogEvent(LogGroup::LOGGER, LogCategory::VERB2,
                                      l.str()));
                IdleCheck_RefDec();
                g_dbus_method_invocation_return_value(invoc, NULL);
            }
            else if ("GetSubscriberList" == meth_name)
            {
                GVariantBuilder *bld = g_variant_builder_new(G_VARIANT_TYPE("a(ssss)"));

                if (nullptr == bld)
                {
                    GError *err = g_dbus_error_new_for_dbus_error(
                                    "net.openvpn.v3.error.log",
                                    "Could not generate subscribers list");
                    g_dbus_method_invocation_return_gerror(invoc, err);
                    g_error_free(err);
                    return;
                }

                for (const auto& l : loggers)
                {
                    Logger::Ptr sub = l.second;
                    g_variant_builder_add(bld, "(ssss)",
                                          std::to_string(l.first).c_str(),
                                          sub->GetBusName().c_str(),
                                          sub->GetInterface().c_str(),
                                          sub->GetObjectPath().c_str());

                }
                g_dbus_method_invocation_return_value(invoc,
                                                      GLibUtils::wrapInTuple(bld));
                return;
            }
            else
            {
                std::string qdom = "net.openvpn.v3.error.invalid";
                GError *dbuserr = g_dbus_error_new_for_dbus_error(qdom.c_str(),
                                                                  "Unknown method");
                g_dbus_method_invocation_return_gerror(invoc, dbuserr);
                g_error_free(dbuserr);
                return;
            }
        }
        catch (DBusCredentialsException& excp)
        {
            logwr->AddMeta(meta.str());
            logwr->Write(LogEvent(LogGroup::LOGGER, LogCategory::CRIT,
                                  excp.what()));
            excp.SetDBusError(invoc);
            return;
        }
   }


    /**
     *   Callback which is called each time a LogServiceManager D-Bus
     *   property is being read.
     *
     * @param conn           D-Bus connection this event occurred on
     * @param sender         D-Bus bus name of the requester
     * @param obj_path       D-Bus object path to the object being requested
     * @param intf_name      D-Bus interface of the property being accessed
     * @param property_name  The property name being accessed
     * @param error          A GLib2 GError object if an error occurs
     *
     * @return  Returns a GVariant Glib2 object containing the value of the
     *          requested D-Bus object property.  On errors, NULL must be
     *          returned and the error must be returned via a GError
     *          object.
     */
    virtual GVariant * callback_get_property(
                     GDBusConnection *conn,
                     const std::string sender,
                     const std::string obj_path,
                     const std::string intf_name,
                     const std::string property_name,
                     GError **error)
    {
        try
        {
            IdleCheck_UpdateTimestamp();

            if ("version" == property_name)
            {
                return g_variant_new_string(package_version());
            }
            else if ("log_level" == property_name)
            {
                return g_variant_new_uint32(log_level);
            }
            else if ("log_dbus_details" == property_name)
            {
                return g_variant_new_boolean(logwr->LogMetaEnabled());
            }

            else if ("timestamp" == property_name)
            {
                return g_variant_new_boolean(logwr->TimestampEnabled());
            }
            else if ("num_attached" == property_name)
            {
                return g_variant_new_uint32(loggers.size());
            }
        }
        catch (...)
        {
            g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                        "Unknown error");
        }
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unknown property");
        return NULL;
    }


    /**
     *  Callback method which is used each time a LogServiceManager
     *  property is being modified over the D-Bus.
     *
     * @param conn           D-Bus connection this event occurred on
     * @param sender         D-Bus bus name of the requester
     * @param obj_path       D-Bus object path to the object being requested
     * @param intf_name      D-Bus interface of the property being accessed
     * @param property_name  The property name being accessed
     * @param value          GVariant object containing the value to be stored
     * @param error          A GLib2 GError object if an error occurs
     *
     * @return Will always throw an exception as there are no properties to
     *         modify.
     */
    virtual GVariantBuilder * callback_set_property(
                    GDBusConnection *conn,
                    const std::string sender,
                    const std::string obj_path,
                    const std::string intf_name,
                    const std::string property_name,
                    GVariant *value,
                    GError **error)
    {
        std::stringstream meta;
        meta << "sender=" << sender
             << ", object_path=" << obj_path
             << ", interface=" << intf_name
             << ", property_name=" << property_name;

        try
        {
            IdleCheck_UpdateTimestamp();
            GVariantBuilder *ret = nullptr;

            if ("log_level" == property_name)
            {
                unsigned int new_log_level = g_variant_get_uint32(value);
                if (new_log_level > 6)
                {
                    throw DBusPropertyException(G_IO_ERROR,
                                                G_IO_ERROR_INVALID_DATA,
                                                obj_path, intf_name,
                                                property_name,
                                                "Invalid log level");
                }
                log_level = new_log_level;
                for (const auto& l : loggers)
                {
                    l.second->SetLogLevel(log_level);
                }
                std::stringstream l;
                l << "Log level changed to " << std::to_string(log_level);
                logwr->AddMeta(meta.str());
                logwr->Write(LogEvent(LogGroup::LOGGER, LogCategory::VERB1,
                                      l.str()));
                ret = build_set_property_response(property_name,
                                                   (guint32) log_level);
            }
            else if ("log_dbus_details" == property_name)
            {
                // First check if this will cause a change
                bool newval= g_variant_get_boolean(value);
                if (logwr->LogMetaEnabled() == newval)
                {
                    // Nothing changes ... make some noise about it
                    throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                                obj_path, intf_name,
                                                property_name,
                                                "New value the same as current value");
                }

                // Changing the setting
                logwr->EnableLogMeta(newval);

                // Log the change
                std::stringstream l;
                l << "D-Bus details logging has changed to "
                  << (newval? "enabled" : "disabled");
                logwr->AddMeta(meta.str());
                logwr->Write(LogEvent(LogGroup::LOGGER, LogCategory::VERB1,
                                      l.str()));

                ret = build_set_property_response(property_name, newval);
            }
            else if ("timestamp" == property_name)
            {
                // First check if this will cause a change
                bool newtstamp = g_variant_get_boolean(value);
                if (logwr->TimestampEnabled() == newtstamp)
                {
                    // Nothing changes ... make some noise about it
                    throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                                obj_path, intf_name,
                                                property_name,
                                                "New value the same as current value");
                }

                // Try setting the new timestamp flag value

                logwr->EnableTimestamp(newtstamp);

                // Re-read the value from the LogWriter.  Some LogWriters
                bool timestamp = logwr->TimestampEnabled();
                // might not allow modifying the timestamp flag

                std::stringstream l;
                l << "Timestamp flag "
                  << (newtstamp == timestamp ? "has" : "could not be")
                  << " changed to: "
                  << (newtstamp ? "enabled" : "disabled");

                logwr->AddMeta(meta.str());
                logwr->Write(LogEvent(
                                LogGroup::LOGGER,
                                (newtstamp == timestamp
                                 ? LogCategory::VERB1 : LogCategory::ERROR),
                                l.str()));
                if (newtstamp != timestamp)
                {
                    throw DBusPropertyException(G_IO_ERROR,
                                                G_IO_ERROR_READ_ONLY,
                                                obj_path, intf_name,
                                                property_name,
                                                "Log timestamp is read-only");
                }
                ret = build_set_property_response(property_name,
                                                   timestamp);
            }

            if (!statedir.empty())
            {
                save_state();
            }
            return ret;
        }
        catch (DBusPropertyException&)
        {
            throw;
        }
        catch (DBusException& excp)
        {
            throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                        obj_path, intf_name, property_name,
                                        excp.what());
        }
        throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                    obj_path, intf_name, property_name,
                                    "Invalid property");
    }


private:
    GDBusConnection *dbuscon = nullptr;
    LogWriter *logwr = nullptr;
    std::map<size_t, Logger::Ptr> loggers = {};
    unsigned int log_level;
    std::string statedir;
    std::vector<std::string> allow_list;


    /**
     *  Validate that the sender is on a list of allowed senders.  If the
     *  sender is not allowed, a DBusCredentialsException is thrown.
     *
     * @param sender  Sender of the D-Bus request
     * @param allow   A std::string of the sender's bus name, which will also
     *                be granted access if registered.
     */
    void validate_sender(std::string sender, std::string allow)
    {
        // Extend the basic allow list with the callers bus name as well.
        // This is to ensure the createor of the Logger object can
        // unsubscribe.
        auto chk = allow_list;
        chk.push_back(allow);

        for (const auto& i : chk)
        {
            // Check if the senders unique bus id matches any of the ones
            // we allow
            std::string uniqid;
            try
            {
                uniqid = GetUniqueBusID(i);
            }
            catch (DBusException& excp)
            {
                // Ignore exceptions, most likely it cannot find the
                // well-known bus name it looks for.  So we use the
                // lookup ID from the allow_list instead
                uniqid = i;
            }

            if (uniqid == sender)
            {
                return;
            }
        }

        // No luck ... so we throw a credentials exception
        try
        {
            uid_t sender_uid = GetUID(sender);
            throw DBusCredentialsException(sender_uid,
                                           "net.openvpn.v3.error.acl.denied",
                                           "Access denied");
        }
        catch (DBusException&)
        {
            throw DBusCredentialsException(sender,
                                           "net.openvpn.v3.error.acl.denied",
                                           "Access denied");
        }
    }


    /**
     *  Loads a previously saved state.  The state is typically just
     *  the various properties of log level and what kind of log details
     *  being added to the log
     *
     *  The state file is simple JSON file which may or may not contain
     *  all these settings.
     *
     */
    void load_state()
    {
        std::ifstream statefile(statedir + "/log-service.json");

        if (statefile.eof() || statefile.fail())
        {
            // We ignore situations if the file
            // does not exist or is not readable
            return;
        }
        std::string line;
        std::stringstream buf;
        while (std::getline(statefile, line))
        {
            buf << line << std::endl;
        }

        // Parse buffer into JSON
        Json::Value state;
        buf >> state;

        auto val= state["log_level"];
        if (val.isUInt())
        {
            log_level = val.asUInt();
        }

        val = state["log_dbus_details"];
        if (val.isBool())
        {
            logwr->EnableLogMeta(val.asBool());
        }

        val = state["timestamp"];
        if (val.isBool())
        {
            logwr->EnableTimestamp(val.asBool());
        }
    }


    /**
     *  Saves the state of the current log service settings to a JSON file
     */
    void save_state()
    {
        Json::Value state;
        state["log_level"] = log_level;
        state["log_dbus_details"] = logwr->LogMetaEnabled();
        state["timestamp"] = logwr->TimestampEnabled();

        std::ofstream statefile(statedir + "/log-service.json");
        statefile << state << std::endl;
        statefile.close();
    }
};


/**
 *  Main Log Service handler.  This class establishes the D-Bus log service
 *  for the OpenVPN 3 Linux client.
 */
class LogService : public DBus,
                   public RC<thread_unsafe_refcount>
{
public:
    typedef RCPtr<LogService> Ptr;

    /**
     *  Initialize the Log Service.
     *
     * @param dbuscon  D-Bus connection to use to enable this service
     * @param logwr    Pointer to a LogWriter object which handles log writes
     * @param log_level Unsigned int with initial default log level to use
     */
    LogService(GDBusConnection *dbuscon, LogWriter *logwr,
               unsigned int log_level)
                    : DBus(dbuscon,
                           OpenVPN3DBus_name_log,
                           OpenVPN3DBus_rootp_log,
                           OpenVPN3DBus_interf_log),
                      logwr(logwr),
                      log_level(log_level),
                      statedir("")
    {
    }

    ~LogService()
    {
    }


    /**
     *  Preserves the --state-dir setting, which will be used when
     *  creating the D-Bus service object
     *
     * @param sd  std::string containing the state directory to use
     */
    void SetStateDirectory(std::string sd)
    {
        statedir = sd;
    }

    /**
     *  This callback is called when the service was successfully registered
     *  on the D-Bus.
     */
    void callback_bus_acquired()
    {
        // Once the D-Bus name is registered and acknowledge,
        // register the Log Service Manager object which does the
        // real work.
        logmgr.reset(new LogServiceManager(GetConnection(),
                                           OpenVPN3DBus_rootp_log,
                                           logwr, log_level));
        logmgr->SetStateDirectory(statedir);
        logmgr->RegisterObject(GetConnection());

        if (nullptr != idle_checker)
        {
            logmgr->IdleCheck_Register(idle_checker);
        }
    }

    /**
     *  This is called each time the well-known bus name is successfully
     *  acquired on the D-Bus.
     *
     *  This is not used, as the preparations already happens in
     *  callback_bus_acquired()
     *
     * @param conn     Connection where this event happened
     * @param busname  A string of the acquired bus name
     */
    void callback_name_acquired(GDBusConnection *conn, std::string busname)
    {
    }


    /**
     *  This is called each time the well-known bus name is removed from the
     *  D-Bus.  In our case, we just throw an exception and starts shutting
     *  down.
     *
     * @param conn     Connection where this event happened
     * @param busname  A string of the lost bus name
     */
    void callback_name_lost(GDBusConnection *conn, std::string busname)
    {
        THROW_DBUSEXCEPTION("LogServiceManager",
                            "openvpn3-service-logger could not register '"
                            + busname + "'");
    };

private:
    LogServiceManager::Ptr logmgr;
    LogWriter *logwr;
    unsigned int log_level;
    std::string statedir;
};
