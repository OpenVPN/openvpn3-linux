//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2022         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2022         David Sommerseth <davids@openvpn.net>
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
 * @file   service.hpp
 *
 * @brief  Declaration of classes used by service.cpp
 */


#pragma once

#include <functional>
#include <memory>

#include "dbus/core.hpp"
#include "dbus/connection-creds.hpp"
#include "dbus/object-property.hpp"
#include "log/dbus-log.hpp"

/**
 *  This provides a more generic interface to generate and process
 *  the log tags and hashes used to separate log events from various
 *  attached log senders.
 */
struct LogTag
{
    /**
     *  LogTag contstructor
     *
     * @param sender     std::string of the D-Bus unique bus name (1:xxxx)
     * @param interface  std::string of the D-Bus interface sending events
     *
     */
    LogTag(std::string sender, std::string interface);


    /**
     *  Return a std::string containing the tag to be used with log lines
     *
     *  The structure is: {tag:xxxxxxxxxxx}
     *  where xxxxxxxxxxx is a positive number
     *
     * @return  Returns a std::string containing the tag this sender and
     *           interface will use
     */
    std::string str() const;


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
 *  A LoggerProxy is at its core an extended LogSender class which is
 *  configured at runtime to be called via a Logger object.
 *
 *  A Logger object is created when any service is attaching itself to the
 *  net.openvpn.v3.log service, telling the log service to collect its log
 *  events.
 *
 *  The LoggerProxy is used by other applications ("clients") who wants to
 *  see a copy of the log events.  When a D-Bus application calls the
 *  ProxyLogEvents method, a LoggerProxy object is initiated for this client
 *  and will then be tied to the appropriate Logger object for the requested
 *  backend VPN service (net.openvpn.v3.backends.be$PID).
 *
 *  A LoggerProxy object can only be configured to forward Log and StatusChange
 *  events from backend VPN client services.
 */
class LoggerProxy : public DBusObject,
                    public DBusConnectionCreds,
                    public LogSender
{
public:
    /**
     * LoggerProxy constructor, created by a LogServiceManager object when
     * processing the ProxyLogEvents D-Bus method.
     *
     * @param dbc        GDBusConnection pointer to the current D-Bus connection
     * @param creat      std::string of the D-Bus unique bus name requesting this
     * @param remove_cb  std::function callback run when this object is deleted
     * @param obj_path   std::string with the D-Bus object path of this proxy
     * @param target     std::string of the target where Log/StatusEvent signals will be sent // FIXME: Isn't this the same as creator?
     * @param src_path   std::string of the session path belonging to the VPN session
     * @param src_interf std::string of the interface of the Log/StatusChange signals
     * @param loglvl     unsigned int of the initial log level for to forward
     */
    LoggerProxy(GDBusConnection *dbc,
                const std::string& creat,
                std::function<void()> remove_cb,
                const std::string& obj_path,
                const std::string& target,
                const std::string& src_path,
                const std::string& src_interf,
                const unsigned int loglvl);
    ~LoggerProxy();

    /**
     * Retrieve the D-Bus object path of this proxy object
     */
    const std::string GetObjectPath() const;

    /**
     * Retrieve the D-Bus object path of the VPN session this proxy is tied to
     */
    const std::string GetSessionPath() const;


    void callback_method_call(GDBusConnection *conn,
                              const std::string sender,
                              const std::string obj_path,
                              const std::string intf_name,
                              const std::string meth_name,
                              GVariant *params,
                              GDBusMethodInvocation *invoc) override;

    GVariant* callback_get_property(GDBusConnection *conn,
                                    const std::string sender,
                                    const std::string obj_path,
                                    const std::string intf_name,
                                    const std::string property_name,
                                    GError **error) override;

    GVariantBuilder* callback_set_property(GDBusConnection *conn,
                                           const std::string sender,
                                           const std::string obj_path,
                                           const std::string intf_name,
                                           const std::string property_name,
                                           GVariant *value,
                                           GError **error) override;


private:
    PropertyCollection props;
    GDBusConnection* dbuscon = nullptr;
    std::string creator = {};
    std::function<void()> remove_callback;
    std::string log_target;
    unsigned int log_level = 6;
    std::string session_path = {};

    void check_access(const std::string& sender) const;
};

using LoggerProxyList = std::map<std::string, LoggerProxy*>;
using LoggerSessionsList = std::map<std::string, size_t>;



/**
 *  The LogServiceManager maintains the D-Bus object to be used
 *  when attaching, detaching and otherwise manage the log processing
 *  over D-Bus
 */
class LogServiceManager : public DBusObject,
                          public DBusConnectionCreds
{
public:
    typedef std::unique_ptr<LogServiceManager> Ptr;

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
    LogServiceManager(GDBusConnection *dbcon,
                      const std::string objpath,
                      LogWriter *logwr,
                      const unsigned int log_level);

    ~LogServiceManager() = default;


    /**
     *  If called, the LogServiceManager will save each settings change to
     *  a state file in this directory.  When called it will attempt to
     *  load a possibly previous saved state file.
     *
     * @param sd  std::string containing the directory where to save the state
     */
    void SetStateDirectory(std::string sd);


    void callback_method_call(GDBusConnection *conn,
                              const std::string sender,
                              const std::string obj_path,
                              const std::string intf_name,
                              const std::string meth_name,
                              GVariant *params,
                              GDBusMethodInvocation *invoc) override;

    GVariant* callback_get_property(GDBusConnection *conn,
                                    const std::string sender,
                                    const std::string obj_path,
                                    const std::string intf_name,
                                    const std::string property_name,
                                    GError **error) override;

    GVariantBuilder* callback_set_property(GDBusConnection *conn,
                                           const std::string sender,
                                           const std::string obj_path,
                                           const std::string intf_name,
                                           const std::string property_name,
                                           GVariant *value,
                                           GError **error) override;


private:
    GDBusConnection *dbuscon = nullptr;
    LogWriter *logwr = nullptr;
    std::map<size_t, Logger::Ptr> loggers = {};
    unsigned int log_level;
    std::string statedir;
    std::vector<std::string> allow_list;
    LoggerProxyList logproxies;
    LoggerSessionsList logger_session = {};

    /**
     *  Validate that the sender is on a list of allowed senders.  If the
     *  sender is not allowed, a DBusCredentialsException is thrown.
     *
     * @param sender  Sender of the D-Bus request
     * @param allow   A std::string of the sender's bus name, which will also
     *                be granted access if registered.
     */
    void validate_sender(std::string sender, std::string allow);


    /**
     *  Loads a previously saved state.  The state is typically just
     *  the various properties of log level and what kind of log details
     *  being added to the log
     *
     *  The state file is simple JSON file which may or may not contain
     *  all these settings.
     *
     */
    void load_state();


    /**
     *  Saves the state of the current log service settings to a JSON file
     */
    void save_state();

    std::string check_busname_vpn_client(const std::string& chk_busn) const;

    std::string add_log_proxy(GVariant *params, const std::string& sender);
    void remove_log_proxy(const std::string target);

};



/**
 *  Main Log Service handler.  This class establishes the D-Bus log service
 *  for the OpenVPN 3 Linux client.
 */
class LogService : public DBus
{
public:
    typedef std::unique_ptr<LogService> Ptr;

    /**
     *  Initialize the Log Service.
     *
     * @param dbuscon  D-Bus connection to use to enable this service
     * @param logwr    Pointer to a LogWriter object which handles log writes
     * @param log_level Unsigned int with initial default log level to use
     */
    LogService(GDBusConnection *dbuscon,
               LogWriter *logwr,
               unsigned int log_level);

    ~LogService() = default;

    /**
     *  Preserves the --state-dir setting, which will be used when
     *  creating the D-Bus service object
     *
     * @param sd  std::string containing the state directory to use
     */
    void SetStateDirectory(std::string sd);

    /**
     *  This callback is called when the service was successfully registered
     *  on the D-Bus.
     */
    void callback_bus_acquired();

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
    void callback_name_acquired(GDBusConnection *conn, std::string busname);


    /**
     *  This is called each time the well-known bus name is removed from the
     *  D-Bus.  In our case, we just throw an exception and starts shutting
     *  down.
     *
     * @param conn     Connection where this event happened
     * @param busname  A string of the lost bus name
     */
    void callback_name_lost(GDBusConnection *conn, std::string busname);

private:
    LogServiceManager::Ptr logmgr;
    LogWriter *logwr;
    unsigned int log_level;
    std::string statedir;
};
