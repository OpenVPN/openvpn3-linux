//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>

#include <openvpn/common/string.hpp>
#include "constants.hpp"
#include "modules/built-in.hpp"
#include "modulehandler.hpp"
#include "service.hpp"


namespace DevPosture {

Handler::Handler(DBus::Connection::Ptr dbuscon,
                 DBus::Object::Manager::Ptr object_manager,
                 LogWriter::Ptr logwr,
                 uint8_t log_level)
    : DBus::Object::Base(PATH_DEVPOSTURE, INTERFACE_DEVPOSTURE),
      object_manager_(object_manager)
{
    signals_ = DevPosture::Log::Create(dbuscon,
                                       LogGroup::EXTSERVICE,
                                       GetPath(),
                                       logwr);
    signals_->SetLogLevel(log_level);
    RegisterSignals(signals_);

    auto grm_args = AddMethod("GetRegisteredModules",
                              [this](DBus::Object::Method::Arguments::Ptr args)
                              {
                                  method_get_registered_modules(args);
                              });

    grm_args->AddOutput("paths", "ao");

    auto pl_args = AddMethod("ProtocolLookup",
                             [this](DBus::Object::Method::Arguments::Ptr args)
                             {
                                 method_protocol_lookup(args);
                             });

    pl_args->AddInput("enterprise_profile", glib2::DataType::DBus<std::string>());
    pl_args->AddOutput("protocol", glib2::DataType::DBus<std::string>());

    auto rc_args = AddMethod("RunChecks",
                             [this](DBus::Object::Method::Arguments::Ptr args)
                             {
                                 method_run_checks(args);
                             });

    rc_args->AddInput("protocol", glib2::DataType::DBus<std::string>());
    rc_args->AddInput("request", glib2::DataType::DBus<std::string>());
    rc_args->AddOutput("result", glib2::DataType::DBus<std::string>());
}


const bool Handler::Authorize(const DBus::Authz::Request::Ptr authzreq)
{
    return true;
}


void Handler::LoadProtocolProfiles(const std::string &profile_dir)
{
    namespace fs = std::filesystem;

    try
    {
        for (const auto &entry : fs::directory_iterator(profile_dir))
        {
            auto p = entry.path();

            if (p.extension() == ".json")
            {
                std::ifstream profile_stream(p, std::ios::in | std::ios::binary);
                Json::Value profile_json;

                profile_stream >> profile_json;

                profiles_[p.stem()] = profile_json;
            }
        }
    }
    catch (const std::exception &e)
    {
        using namespace std::string_literals;

        signals_->LogError("Error parsing protocol profiles: "s + e.what());
    }
}


Json::Value Handler::compose_json(const Module::Dictionary &dict, const Json::Value &result_mapping) const
{
    Json::Value ret;

    for (auto it = result_mapping.begin(); it != result_mapping.end(); ++it)
    {
        if (!it->isObject())
        {
            std::string val;
            auto dict_it = dict.find(it->asString());

            if (dict_it != dict.end())
                val = dict_it->second;

            ret[it.key().asString()] = val;
        }
        else
        {
            ret[it.key().asString()] = compose_json(dict, *it);
        }
    }

    return ret;
}


void Handler::method_get_registered_modules(DBus::Object::Method::Arguments::Ptr args) const
{
    DBus::Object::Path::List paths;

    for (const auto &[path, object] : object_manager_->GetAllObjects())
    {
        // Don't return the root module.
        if (std::dynamic_pointer_cast<ModuleHandler>(object))
        {
            paths.push_back(path);
        }
    }

    args->SetMethodReturn(glib2::Value::CreateTupleWrapped(paths));
}


void Handler::method_protocol_lookup(DBus::Object::Method::Arguments::Ptr args) const
{
    GVariant *params = args->GetMethodParameters();
    auto enterprise_profile = glib2::Value::Extract<std::string>(params, 0);

    std::string retval;
    auto it = profiles_.find(enterprise_profile);

    if (it == profiles_.end())
    {
        const std::string err_msg = "ProtocolLookup(): Could not find the appcontrol_id for enterprise profile '"
                                    + enterprise_profile + "'";

        signals_->LogError(err_msg);
        throw DBus::Object::Method::Exception(err_msg);
    }

    retval = it->second["appcontrol_id"].asString();
    args->SetMethodReturn(glib2::Value::CreateTupleWrapped(retval));
}


void Handler::method_run_checks(DBus::Object::Method::Arguments::Ptr args) const
{
    using namespace std::chrono;

    GVariant *params = args->GetMethodParameters();

    const auto protocol = glib2::Value::Extract<std::string>(params, 0);
    const auto request = glib2::Value::Extract<std::string>(params, 1);

    std::string ret_string;
    Json::Value request_json;

    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;

    std::string errors;
    std::istringstream instr(request);

    if (!Json::parseFromStream(builder, instr, &request_json, &errors))
    {
        throw DBus::Object::Method::Exception("devposture: invalid request JSON: " + request);
    }

    std::string correlation_id;
    std::string version;

    std::set<std::string> checks;

    const auto &req_data = request_json["dpc_request"];

    for (auto cit = req_data.begin(); cit != req_data.end(); ++cit)
    {
        const std::string key = cit.key().asString();

        if (key == "correlation_id")
        {
            correlation_id = cit->asString();
        }
        else if (key == "ver")
        {
            version = cit->asString();
        }
        else if (key != "timestamp")
        {
            if (cit->isBool() && cit->asBool())
            {
                checks.insert(key);
            }
        }
    }

    Json::Value ret_json;
    Json::Value ret_payload_json;
    bool protocol_found = false;
    bool version_matches = false;

    for (auto &&[enterprise_id, json_representation] : profiles_)
    {
        auto protocols =
            openvpn::string::split(json_representation["appcontrol_id"].asString(), ':');

        if (std::find(protocols.begin(), protocols.end(), protocol) == protocols.end())
        {
            continue;
        }

        protocol_found = true;

        if (json_representation["ver"].asString() != version)
        {
            continue;
        }

        version_matches = true;

        const auto &mappings = json_representation["control_mapping"];

        for (auto it = mappings.begin(); it != mappings.end(); ++it)
        {
            const auto &mapping_data = *it;
            Json::Value result_json;

            if (checks.find(it.key().asString()) != checks.end())
            {
                if (mapping_data.isArray())
                {
                    result_json = create_merged_mapped_json(mapping_data);
                }
                else
                {
                    result_json = create_mapped_json(mapping_data);
                }

                ret_payload_json[it.key().asString()] = result_json;
            }
        }

        if (!ret_payload_json.empty())
        {
            auto &dpc_response = ret_json["dpc_response"];

            dpc_response["ver"] = version;
            dpc_response["correlation_id"] = correlation_id;
            dpc_response["timestamp"] = generate_timestamp();

            for (auto it = ret_payload_json.begin(); it != ret_payload_json.end(); ++it)
            {
                dpc_response[it.key().asString()] = *it;
            }

            Json::StreamWriterBuilder builder;
            builder.settings_["indentation"] = "";
            ret_string = Json::writeString(builder, ret_json);
        }

        break;
    }

    if (!protocol_found || !version_matches || !ret_json)
    {
        signals_->Debug("Could not handle dpc_request: " + request);
    }

    if (!protocol_found)
    {
        const std::string err_msg = "protocol '" + protocol
                                    + "' not supported [correlation_id: "
                                    + correlation_id + "]";

        signals_->LogError("RunChecks(): " + err_msg);
        throw DBus::Object::Method::Exception(err_msg);
    }

    if (!version_matches)
    {
        const std::string err_msg = "requested version '" + version
                                    + "' not supported [correlation_id: "
                                    + correlation_id + "]";

        signals_->LogError("RunChecks(): " + err_msg);
        throw DBus::Object::Method::Exception(err_msg);
    }

    if (!ret_json)
    {
        const std::string err_msg = "no supported checks found [correlation_id: "
                                    + correlation_id + "]";

        signals_->LogError("RunChecks(): " + err_msg);
        throw DBus::Object::Method::Exception(err_msg);
    }

    signals_->LogVerb2("RunChecks(\"" + protocol + "\", \"" + request + "\") -> \""
                       + ret_string + "\"");

    args->SetMethodReturn(glib2::Value::CreateTupleWrapped(ret_string));
}


std::string Handler::generate_timestamp()
{
    using namespace std::chrono;

    const auto now = system_clock::now();
    const auto now_time_t = system_clock::to_time_t(now);
    const auto ms = time_point_cast<milliseconds>(now).time_since_epoch().count() % 1000;

    tm utc_time{};

    gmtime_r(&now_time_t, &utc_time);

    std::stringstream ss;

    ss << std::put_time(&utc_time, "%a %b %d %T.") << std::setfill('0')
       << std::setw(3) << ms << std::put_time(&utc_time, " %Y");

    return ss.str();
}


Json::Value Handler::create_mapped_json(const Json::Value &mapping_data) const
{
    const std::string module_path = mapping_data["module"].asString();
    const auto module_object = object_manager_->GetObject<ModuleHandler>(module_path);

    if (module_object)
    {
        const auto dict = module_object->Run({});

        return compose_json(dict, mapping_data["result_mapping"]);
    }

    return {};
}


Json::Value Handler::create_merged_mapped_json(const Json::Value &arr_mapping_data) const
{
    Json::Value ret;

    for (const auto &elem : arr_mapping_data)
    {
        const Json::Value mapped_json = create_mapped_json(elem);

        if (!mapped_json.empty())
        {
            for (auto it = mapped_json.begin(); it != mapped_json.end(); ++it)
            {
                ret[it.key().asString()] = *it;
            }
        }
    }

    return ret;
}


Service::Service(DBus::Connection::Ptr dbuscon, LogWriter::Ptr logwr, uint8_t log_level)
    : DBus::Service(dbuscon, SERVICE_DEVPOSTURE), dbuscon_(dbuscon),
      logwr_(std::move(logwr)), log_level_(log_level)
{
    try
    {
        logsrvprx_ = LogServiceProxy::AttachInterface(dbuscon, INTERFACE_DEVPOSTURE);
        handler_ = CreateServiceHandler<Handler>(dbuscon_, GetObjectManager(), logwr_, log_level_);
    }
    catch (const DBus::Exception &excp)
    {
        logwr_->Write(LogGroup::CONFIGMGR,
                      LogCategory::CRIT,
                      excp.GetRawError());
    }
}


Service::~Service() noexcept
{
    if (logsrvprx_)
    {
        logsrvprx_->Detach(INTERFACE_DEVPOSTURE);
    }
}


void Service::LoadProtocolProfiles(const std::string &profile_dir)
{
    handler_->LoadProtocolProfiles(profile_dir);
}


void Service::BusNameAcquired(const std::string &busname)
{
    auto object_manager = GetObjectManager();

    object_manager->CreateObject<ModuleHandler>(Module::Create<PlatformModule>(), false);
    object_manager->CreateObject<ModuleHandler>(Module::Create<DateTimeModule>(), false);
}


void Service::BusNameLost(const std::string &busname)
{
    throw DBus::Service::Exception(
        "openvpn3-service-devposture lost the '" + busname
        + "' registration on the D-Bus");
}

} // end of namespace DevPosture
