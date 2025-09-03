//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

#include <gdbuspp/connection.hpp>
#include <gdbuspp/service.hpp>
#include <json/json.h>
#include <log/proxy-log.hpp>

#include <dbus/constants.hpp>
#include "devposture-signals.hpp"
#include "modules/module-interface.hpp"


namespace DevPosture {

/**
 *  Main D-Bus service handler object
 *
 *  Provides the entry object to access the device posture functionality
 */
class Handler : public DBus::Object::Base
{
  public:
    using Ptr = std::shared_ptr<Handler>;

  public:
    Handler(DBus::Connection::Ptr dbuscon,
            DBus::Object::Manager::Ptr object_manager,
            LogWriter::Ptr logwr,
            uint8_t log_level);

    const bool Authorize(const DBus::Authz::Request::Ptr authzreq) override;

    /**
     *  Loads all the device posture protocol profile definitions
     *
     * @param profile_dir  std::string containing the directory of all the
     *                     available protocol profile definitions
     */
    void LoadProtocolProfiles(const std::string &profile_dir);

  private:
    /**
     * Composes a Json::Value (JsonCPP) JSON object, by replacing all
     * macros in result_mapping with the corresponding value in the
     * dict key -> value container. For example, if result_mapping
     * contains { "type": "uname_sysname" }, and dict has a [key, value]
     * element where key == "uname_sysname" and value == "Linux", then
     * the resulting JSON will have { "type": "Linux" }.
     *
     * @param dict            key -> value "macro" association container.
     * @param result_mapping  A "raw" JSON where "macros" need substitution.
     *
     * @return Json::Value    A JSON where all the macros have been
     *                        substituted.
     */
    Json::Value compose_json(const Module::Dictionary &dict, const Json::Value &result_mapping) const;

    /**
     *  D-Bus method: net.openvpn.v3.devposture.GetRegisteredModules
     *
     *  Provides a list of all the available device posture check modules.
     *  The return value is an array of D-Bus path per module.
     *
     *  Input:  n/a
     *
     *  Output: (ao)
     *    o - module_path:   D-Bus path to the device posture module
     *
     * @param args DBus::Object::Method::Arguments
     */
    void method_get_registered_modules(DBus::Object::Method::Arguments::Ptr args) const;

    /**
     *  D-Bus method: net.openvpn.v3.devposture.ProtocolLookup
     *
     *  Returns the device posture protocol name for a configuration profile's
     *  Enterprise Profile ID (EPID).
     *
     *  There is a mapping between the EPID and the device posture protocol
     *  the server side expects to receive.  The EPID can be used across
     *  more VPN configuration profiles, and the configuration profiles which
     *  should return a device posture check need to have an EPID assigned.
     *
     *  Input:  (s)
     *    s - enterprise_profile:  The EPID to look up
     *
     *  Output: (s)
     *    o - protocol:            The device posture protocol to use
     *
     * @param args DBus::Object::Method::Arguments
     */
    void method_protocol_lookup(DBus::Object::Method::Arguments::Ptr args) const;

    /**
     *  D-Bus method: net.openvpn.v3.devposture.RunChecks
     *
     *  Runs a device posture check for a specific device posture protocol.
     *  This method expects a JSON formatted request object to be passed to
     *  it, which contains an overview of which checks to perform and report
     *  back.
     *
     *  Example request:
     *
     *  {
     *    "dpc_request": {
     *      "ver": "1.0",
     *      "correlation_id": "0889817969045489535",
     *      "timestamp": "Mon Jun 17 21:48:19.821 2024",
     *      "client_info": true
     *    }
     *  }
     *
     *  Input:  (ss)
     *    s - protocol:      Device Posture protocol to use
     *    s - request:       String containing the JSON request object
     *
     *  Output: (ao)
     *    o - result:        String containing a JSON result of the checks
     *                       performed
     *
     * @param args DBus::Object::Method::Arguments
     */
    void method_run_checks(DBus::Object::Method::Arguments::Ptr args) const;

    /**
     *  Generates a human-readable date/timestamp used in the dpc_response
     *  back to the server
     *
     * @return std::string
     */
    static std::string generate_timestamp();

    /**
     *  Uses the module specified in mapping_data to compose a JSON
     *  by using the module's dictionary output and the "result_mapping"
     *  sub-JSON of mapping_data as parameters to compose_json().
     *
     * @param mapping_data  A "parent" JSON with information about a
     *                      module to use for retrieving information,
     *                      and a sub-JSON that needs to have its
     *                      macros substituted.
     *
     * @return Json::Value  A JSON with all the macros substituted.
     */
    Json::Value create_mapped_json(const Json::Value &mapping_data) const;

    /**
     *  Uses the create_mapped_json() member function for each element
     *  of the arr_mapping_data array, then merges the results into
     *  the returned value.
     *
     * @param mapping_data  An array of "parent" JSONs, each with
     *                      information about a module to use for
     *                      retrieving information, and a sub-JSON
     *                      that needs to have its macros substituted.
     *
     * @return Json::Value  A JSON with all the macros substituted.
     */
    Json::Value create_merged_mapped_json(const Json::Value &arr_mapping_data) const;

  private:
    DBus::Object::Manager::Ptr object_manager_;
    DevPosture::Log::Ptr signals_;
    std::map<std::string, Json::Value> profiles_;
};


class Service : public DBus::Service
{
  public:
    using Ptr = std::shared_ptr<Service>;

  public:
    Service(DBus::Connection::Ptr dbuscon,
            LogWriter::Ptr logwr,
            uint8_t log_level);

    ~Service() noexcept;

    void BusNameAcquired(const std::string &busname) override;
    void BusNameLost(const std::string &busname) override;

    /**
     *  Loads all the device posture protocol profile definitions from the
     *  given directory
     *
     * @param profile_dir  std::string containing the directory of all the
     *                     available protocol profile definitions
     */
    void LoadProtocolProfiles(const std::string &profile_dir);

  private:
    DBus::Connection::Ptr dbuscon_;
    LogServiceProxy::Ptr logsrvprx_;
    LogWriter::Ptr logwr_;
    uint8_t log_level_;
    Handler::Ptr handler_ = nullptr;
};

} // end of namespace DevPosture
