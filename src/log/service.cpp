//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2022  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2022  David Sommerseth <davids@openvpn.net>
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
 * @file   service.cpp
 *
 * @brief  D-Bus service for log management
 */

#include <map>
#include <string>
#include <functional>
#include <json/json.h>

#include "common/utils.hpp"
#include "dbus/core.hpp"
#include "dbus/exceptions.hpp"
#include "dbus/connection-creds.hpp"
#include "dbus/glibutils.hpp"
#include "dbus/path.hpp"
#include "logger.hpp"
#include "dbus-log.hpp"
#include "service.hpp"

using namespace openvpn;


//
//  LogTag class implementation
//

LogTag::LogTag(std::string sender, std::string interface)
{
    tag = std::string("[") + sender + "/" + interface + "]";

    // Create a hash of the tag, used as an index
    std::hash<std::string> hashfunc;
    hash = hashfunc(tag);
}


std::string LogTag::str() const
{
    return std::string("{tag:") + std::string(std::to_string(hash))
           + std::string("}");
}



//
//  LoggerProxy class implementation
//
LoggerProxy::LoggerProxy(GDBusConnection *dbc,
                         const std::string& creat,
                         std::function<void()> remove_cb,
                         const std::string& obj_path,
                         const std::string& target,
                         const std::string& src_path,
                         const std::string& src_interf,
                         const unsigned int loglvl)
    : DBusObject(obj_path),
      DBusConnectionCreds(dbc),
      LogSender(dbc, LogGroup::UNDEFINED, src_interf, src_path, nullptr),
      props(this),
      dbuscon(dbc), creator(creat), remove_callback(remove_cb),
      log_target(target), log_level(loglvl), session_path(src_path)
{
    props.AddBinding(new PropertyType<unsigned int>(
            this, "log_level", "readwrite", true, log_level));
    props.AddBinding(new PropertyType<std::string>(
            this, "target", "read", true, log_target));
    props.AddBinding(new PropertyType<std::string>(
            this, "session_path", "read", true, session_path));

    std::stringstream introspection_xml;
    introspection_xml << "<node name='" << obj_path << "'>"
            << "    <interface name='" << OpenVPN3DBus_interf_log << "'>"
            << "        <method name='Remove'/>"
            << GetLogIntrospection()
            << props.GetIntrospectionXML()
            << "    </interface>"
            << "</node>";

    ParseIntrospectionXML(introspection_xml);

    SetLogLevel(6);
    AddTargetBusName(target);
    AddPathFilter(src_path);
    RegisterObject(dbc);
}


LoggerProxy::~LoggerProxy()
{
    remove_callback();
}


const std::string LoggerProxy::GetObjectPath() const
{
    return DBusObject::GetObjectPath();
}


const std::string LoggerProxy::GetSessionPath() const
{
    return session_path;
}


void LoggerProxy::callback_method_call(GDBusConnection *conn,
                                       const std::string sender,
                                       const std::string obj_path,
                                       const std::string intf_name,
                                       const std::string meth_name,
                                       GVariant *params,
                                       GDBusMethodInvocation *invoc)
{
    try
    {
        check_access(sender);

        if ("Remove" == meth_name)
        {
            RemoveObject(conn);
            delete this;
            g_dbus_method_invocation_return_value(invoc, NULL);
            return;
        }
    }
    catch (DBusCredentialsException& excp)
    {
        excp.SetDBusError(invoc);
    }
}

GVariant* LoggerProxy::callback_get_property(GDBusConnection *conn,
                                             const std::string sender,
                                             const std::string obj_path,
                                             const std::string intf_name,
                                             const std::string property_name,
                                             GError **error)
{
    try
    {
        check_access(sender);

        if (props.Exists(property_name))
        {
            return props.GetValue(property_name);
        }
    }
    catch (DBusCredentialsException& excp)
    {
        excp.SetDBusError(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED);
    }

    return nullptr;
}


GVariantBuilder* LoggerProxy::callback_set_property(GDBusConnection *conn,
                                                    const std::string sender,
                                                    const std::string obj_path,
                                                    const std::string intf_name,
                                                    const std::string property_name,
                                                    GVariant *value,
                                                    GError **error)
{
    try
    {
        check_access(sender);
    }
    catch (DBusCredentialsException& excp)
    {
        excp.SetDBusError(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED);
    }
    return nullptr;
}


void LoggerProxy::check_access(const std::string& sender) const
{
    // Only allow the creator of this proxy (typically the session manager)
    // and the root user access
    if ((sender != creator) && 0 == GetUID(sender))
    {
        throw DBusCredentialsException(sender, "net.openvpn.v3.log.acl",
                                    "Access Denied,  invalid caller.");
    }
}


//
//  LogServiceManager class implementation
//
LogServiceManager::LogServiceManager(GDBusConnection *dbcon,
                                     const std::string objpath,
                                     LogWriter *logwr,
                                     const unsigned int log_level)
        : DBusObject(objpath), DBusConnectionCreds(dbcon),
          dbuscon(dbcon), logwr(logwr), log_level(log_level), statedir("")
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
    << "        <method name='AssignSession'>"
    << "            <arg type='o' name='session_path' direction='in'/>"
    << "            <arg type='s' name='interface' direction='in'/>"
    << "        </method>"
    << "        <method name='Detach'>"
    << "            <arg type='s' name='interface' direction='in'/>"
    << "        </method>"
    << "        <method name='GetSubscriberList'>"
    << "            <arg type='a(ssss)' name='subscribers' direction='out'/>"
    << "        </method>"
    << "        <method name='ProxyLogEvents'>"
    << "            <arg type='s' name='target_address' direction='in'/>"
    << "            <arg type='o' name='session_path' direction='in'/>"
    << "            <arg type='o' name='proxy_path' direction='out'/>"
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


void LogServiceManager::SetStateDirectory(std::string sd)
{
    if (sd.empty())
    {
        // Ignore this
        return;
    }
    statedir = sd;
    load_state();
}


void LogServiceManager::callback_method_call(GDBusConnection *conn,
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
        if ("GetSubscriberList" != meth_name
            && "ProxyLogEvents" != meth_name
            && "AssignSession" != meth_name)
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
        else if ("AssignSession" == meth_name)
        {
            // Check if the sender is a VPN client. If it is, save an
            // index entry to the LogTag hash of the logger using the
            // session path as the lookup key.
            //
            GLibUtils::checkParams(__func__, params, "(os)", 2);
            std::string sesspath = GLibUtils::ExtractValue<std::string>(params, 0);
            interface = GLibUtils::ExtractValue<std::string>(params, 1);

            std::string be_busname = check_busname_vpn_client(sender);
            if (!be_busname.empty())
            {
                LogTag tag(sender, interface);
                logger_session[sesspath] = tag.hash;
                logwr->Write(LogEvent(LogGroup::LOGGER, LogCategory::DEBUG,
                                      "Assigned session " + sesspath
                                      + " to " + tag.str()));
                g_dbus_method_invocation_return_value(invoc, NULL);
                return;
            }
            else
            {
                logwr->AddMeta(meta.str());
                logwr->Write("AssignSession caller (" + sender + ")"
                             + " is not a VPN client process");
                GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.log",
                                                              "Caller is not a VPN client");
                g_dbus_method_invocation_return_gerror(invoc, err);
                g_error_free(err);
                return;

            }

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

            // Do a reverse lookup in logger_session to retrieve
            // the key to delete from the logger_sesion index
            auto it = std::find_if(logger_session.begin(),
                                  logger_session.end(),
                                  [tag](const auto& e)
                                  {
                                      return e.second == tag.hash;
                                  });
            if (logger_session.end() != it)
            {
                logger_session.erase(it->first);
            }


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
        else if ("ProxyLogEvents" == meth_name)
        {
            try
            {
                std::string p = add_log_proxy(params, sender);
                g_dbus_method_invocation_return_value(invoc,
                                                      g_variant_new("(o)",
                                                              p.c_str()));
            }
            catch (DBusException& err)
            {
                err.SetDBusError(invoc, "net.openvpn.v3.log.proxy.error");
            }
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


GVariant* LogServiceManager::callback_get_property(GDBusConnection *conn,
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



GVariantBuilder* LogServiceManager::callback_set_property(GDBusConnection *conn,
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


void LogServiceManager::validate_sender(std::string sender, std::string allow)
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


void LogServiceManager::load_state()
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


void LogServiceManager::save_state()
{
    Json::Value state;
    state["log_level"] = log_level;
    state["log_dbus_details"] = logwr->LogMetaEnabled();
    state["timestamp"] = logwr->TimestampEnabled();

    std::ofstream statefile(statedir + "/log-service.json");
    statefile << state << std::endl;
    statefile.close();
}


std::string LogServiceManager::check_busname_vpn_client(const std::string& chk_busn) const
{
    // All VPN backend client processes has a well-known D-Bus name which
    // starts with:
    //     OpenVPN3DBus_name_backends_be + pid_of_process
    //
    // The busname given here can be both a well-known D-Bus name or a
    // unique D-Bus name (":1.xxxx").
    //
    // To identify if this is a client, first look up the PID of the given
    // busname, then build a well-known busname string using that PID value
    // and query for the unique busname using the generated well-known busname
    // string.  If this matches the given busname to this method, it is a
    // backend client process.

    try
    {
        pid_t pid = DBusConnectionCreds::GetPID(chk_busn);
        std::string well_known_name = OpenVPN3DBus_name_backends_be
                                    + std::to_string(pid);
        std::string verify_name = DBusConnectionCreds::GetUniqueBusID(well_known_name);
        return (verify_name == chk_busn ? well_known_name : "");
    }
    catch (const DBusException&)
    {
        // Well-known bus name not found
        return "";
    }
}


std::string LogServiceManager::add_log_proxy(GVariant *params, const std::string& sender)
{
    GLibUtils::checkParams(__func__, params, "(so)", 2);
    const std::string target = GLibUtils::ExtractValue<std::string>(params, 0);
    std::string session_path = GLibUtils::ExtractValue<std::string>(params, 1);

    // Check if the session path is known
    size_t log_tag{};
    try
    {
        log_tag = logger_session.at(session_path);
    }
    catch (const std::out_of_range&)
    {
        std::string err = "No VPN session exists with '" + session_path +"'";
        logwr->Write(LogEvent(LogGroup::LOGGER, LogCategory::WARN,
                     "ProxyLogEvents(" + target + "): " + err));
        THROW_DBUSEXCEPTION("LogServiceManager", err);
    }


    std::string path = generate_path_uuid(OpenVPN3DBus_rootp_log + "/proxy", 'l');


    auto rm_callback = [self=(LogServiceManager*) this, target]()
                    {
                        self->remove_log_proxy(target);
                    };
    LoggerProxy* logprx = new LoggerProxy(GetConnection(),
                                         sender, rm_callback,
                                         path, target,
                                         session_path, OpenVPN3DBus_interf_backends,
                                         log_level);

    IdleCheck_RefInc();
    logproxies[target] = logprx;

    // Enable log forwarding in main Logger object for client session
    loggers[log_tag]->AddLogForward(logprx, target);

    logwr->Write(LogEvent(LogGroup::LOGGER, LogCategory::VERB1,
                          "Added new log proxy by " + sender
                          + " - session: " + session_path
                          + ", target: " + target + ", tag: " + std::to_string(log_tag)));

    return logproxies[target]->GetObjectPath();
}

void LogServiceManager::remove_log_proxy(const std::string target)
{
    // The log forwarding must be removed in the main Logger object.
    // The Logger objects are indexed by its log tag,
    // which can be retrieved by the index lookup in logger_session, but
    // this index is using session path as the key.  The session path is
    // available in the LoggerProxy::GetSessionPath(), which is indexed
    // by the log listener's target address.
    //
    // In reverse, that means:
    //
    //   1.  Get the session_path from LoggerProxy using the _target_ address
    //       which is this methods only input argument.
    //   2.  Retrieve the log_tag from the session path
    //       (using the logger_session index)
    //   3.  Execute the proper Logger::RemoveLogForward(), based on the
    //       log_tag
    //
    std::string session_path{"(not found)"};
    size_t log_tag{0};
    try
    {
        session_path = logproxies.at(target)->GetSessionPath();
        log_tag = logger_session.at(session_path);
        loggers.at(log_tag)->RemoveLogForward(target);
#if OPENVPN_DEBUG
        logwr->Write(LogGroup::LOGGER, LogCategory::DEBUG,
                     std::string("remove_log_proxy: ")
                     + "target=" + target + ", "
                     + "session_path=" + session_path + ", "
                     + "log_tag=" + std::to_string(log_tag));
#endif

    }
    catch (const std::out_of_range&)
    {
        // If the log_tag was not found, the VPN client backend process
        // has already detached itself from the lgo service.
        if (log_tag > 0)
        {
            logwr->Write(LogGroup::LOGGER, LogCategory::WARN,
                         std::string("Could not find session/logger for RemoveLogForward() call: ")
                         + "target=" + target + ", "
                         + "session_path=" + session_path + ", "
                         + "log_tag=" + std::to_string(log_tag));
        }
    }

    // With the log forwarding removed in the Logger object, we can
    // erase the LoggerProxy object.
    logproxies.erase(target);
    logwr->Write(LogEvent(LogGroup::LOGGER, LogCategory::VERB1,
                          "Removed log proxy: " + target));
    IdleCheck_RefDec();
}


//
//  LogService class implementation
//

LogService::LogService(GDBusConnection *dbuscon,
                       LogWriter *logwr,
                       unsigned int log_level)
        : DBus(dbuscon, OpenVPN3DBus_name_log, OpenVPN3DBus_rootp_log,
                OpenVPN3DBus_interf_log),
          logwr(logwr), log_level(log_level), statedir("")
{
}


void LogService::SetStateDirectory(std::string sd)
{
    statedir = sd;
}


void LogService::callback_bus_acquired()
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


void LogService::callback_name_acquired(GDBusConnection *conn,
                                        std::string busname)
{
}


void LogService::callback_name_lost(GDBusConnection *conn, std::string busname)
{
    THROW_DBUSEXCEPTION("LogServiceManager",
                        "openvpn3-service-logger could not register '"
                        + busname + "'");
};
