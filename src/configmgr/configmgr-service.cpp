//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018-  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2018-  Lev Stipakov <lev@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

/**
 * @file configmgr-service.cpp
 *
 * @brief Implementation of the net.openvpn.v3.configuration D-Bus service
 */

#include "common/lookup.hpp"
#include "dbus/path.hpp"
#include "configmgr-service.hpp"
#include "configmgr-exceptions.hpp"
#include "constants.hpp"


namespace ConfigManager {

// ConfigHandler

ConfigHandler::ConfigHandler(DBus::Connection::Ptr dbuscon,
                             DBus::Object::Manager::Ptr object_manager,
                             uint8_t loglevel,
                             LogWriter::Ptr logwr)
    : DBus::Object::Base(PATH_CONFIGMGR, INTERFACE_CONFIGMGR),
      dbuscon_(dbuscon), object_manager_(object_manager),
      creds_qry_(DBus::Credentials::Query::Create(dbuscon)),
      logwr_(logwr)
{
    DisableIdleDetector(true);

    signals_ = ConfigManager::Log::Create(dbuscon,
                                          LogGroup::CONFIGMGR,
                                          GetPath(),
                                          logwr);
    signals_->SetLogLevel(loglevel);
    RegisterSignals(signals_);
    CoreLog::Connect(signals_);

    signals_->GroupCreate("broadcast");
    signals_->GroupAddTarget("broadcast", "");
    sig_configmgr_event_ = signals_->GroupCreateSignal<::Signals::ConfigurationManagerEvent>("broadcast");

    auto import_args = AddMethod("Import",
                                 [this](DBus::Object::Method::Arguments::Ptr args)
                                 {
                                     method_import(args);
                                 });

    import_args->AddInput("name", glib2::DataType::DBus<std::string>());
    import_args->AddInput("config_str", glib2::DataType::DBus<std::string>());
    import_args->AddInput("single_use", glib2::DataType::DBus<bool>());
    import_args->AddInput("persistent", glib2::DataType::DBus<bool>());
    import_args->AddOutput("config_path", "o");

    auto fac_args = AddMethod("FetchAvailableConfigs",
                              [this](DBus::Object::Method::Arguments::Ptr args)
                              {
                                  method_fetch_available_configs(args);
                              });

    fac_args->AddOutput("paths", "ao");

    auto lcn_args = AddMethod("LookupConfigName",
                              [this](DBus::Object::Method::Arguments::Ptr args)
                              {
                                  method_lookup_config_name(args);
                              });

    lcn_args->AddInput("config_name", glib2::DataType::DBus<std::string>());
    lcn_args->AddOutput("config_paths", "ao");

    auto sbt_args = AddMethod("SearchByTag",
                              [this](DBus::Object::Method::Arguments::Ptr args)
                              {
                                  method_search_by_tag(args);
                              });

    sbt_args->AddInput("tag", glib2::DataType::DBus<std::string>());
    sbt_args->AddOutput("paths", "ao");

    auto sbo_args = AddMethod("SearchByOwner",
                              [this](DBus::Object::Method::Arguments::Ptr args)
                              {
                                  method_search_by_owner(args);
                              });

    sbo_args->AddInput("owner", glib2::DataType::DBus<std::string>());
    sbo_args->AddOutput("paths", "ao");

    auto to_args = AddMethod("TransferOwnership",
                             [this](DBus::Object::Method::Arguments::Ptr args)
                             {
                                 method_transfer_ownership(args);
                             });

    to_args->AddInput("path", "o");
    to_args->AddInput("new_owner_uid", glib2::DataType::DBus<uint32_t>());

    AddProperty("version", prop_version_, /* readwrite */ false);
}


const bool ConfigHandler::Authorize(const DBus::Authz::Request::Ptr authzreq)
{
    return true;
}


void ConfigHandler::SetStateDirectory(const std::string &state_dir)
{
    if (!state_dir_.empty())
    {
        throw ConfigManager::Exception("State directory already set");
    }

    state_dir_ = state_dir;

    // Load all the already saved persistent configurations before
    // continuing.
    for (const auto &fname : get_persistent_config_file_list(state_dir_))
    {
        try
        {
            import_persistent_configuration(fname);
        }
        catch (const openvpn::option_error &e)
        {
            signals_->LogCritical("Could not import configuration profile " + fname + ": "
                                  + std::string(e.what()));
        }
        catch (const DBus::Exception &e)
        {
            std::string err(e.what());

            if (err.find("failed: An object is already exported for the interface") != std::string::npos)
            {
                signals_->LogCritical("Could not import persistent configuration: " + fname);
            }
            else
            {
                throw;
            }
        }
        catch (const std::exception &e)
        {
            signals_->LogCritical("Invalid persistent configuration file " + fname
                                  + ": " + e.what());
        }
    }
}


std::vector<std::string> ConfigHandler::get_persistent_config_file_list(const std::string &directory)
{
    DIR *dirfd = nullptr;
    struct dirent *entry = nullptr;

    dirfd = opendir(directory.c_str());
    if (nullptr == dirfd)
    {
        std::stringstream err;
        err << "Cannot open state-dir directory '" << directory << "': "
            << strerror(errno);
        throw ConfigManager::Exception(err.str());
    }

    std::vector<std::string> filelist;
    while (nullptr != (entry = readdir(dirfd)))
    {
        // Filter out filenames not relevant.  The expected format is:
        //    34eea818xe578x4356x924bx9fccbbeb92eb.json
        // which, for simplicity is checked as:
        //     string length == 41 and last 5 characters are ".json"
        std::string fname(entry->d_name);
        if (41 != fname.size()
            || ".json" != fname.substr(36, 5))
        {
            continue;
        }

        std::stringstream fullpath;
        fullpath << directory << "/" << fname;

        // Filter out only files and symbolic links
        struct stat stbuf;
        if (0 == lstat(fullpath.str().c_str(), &stbuf))
        {
            switch (stbuf.st_mode & S_IFMT)
            {
            case S_IFREG:
            case S_IFLNK:
                filelist.push_back(fullpath.str());
                break;

            case S_IFDIR:
                // Ignore directories silently
                break;

            default:
                signals_->LogWarn("Unsupported file type: " + fullpath.str());
                break;
            }
        }
        else
        {
            std::stringstream err;
            err << "Could not access file '" << fullpath.str() << "': "
                << strerror(errno);
            signals_->LogError(err.str());
        }
    }
    closedir(dirfd);

    return filelist;
}


void ConfigHandler::import_persistent_configuration(const std::string &fname)
{
    signals_->LogVerb1("Loading persistent configuration: " + fname);

    // Load the JSON file and parse it
    std::ifstream statefile(fname, std::ifstream::binary);
    Json::Value data;
    statefile >> data;

    object_manager_->CreateObject<Configuration>(dbuscon_,
                                                 object_manager_,
                                                 creds_qry_,
                                                 sig_configmgr_event_,
                                                 fname,
                                                 data,
                                                 signals_->GetLogLevel(),
                                                 logwr_);
}


void ConfigHandler::method_import(DBus::Object::Method::Arguments::Ptr args)
{
    using namespace std::string_literals;

    try
    {
        const DBus::Object::Path config_path = generate_path_uuid(Constants::GenPath(SERVICE_ID), 'x');

        GVariant *params = args->GetMethodParameters();

        auto name = glib2::Value::Extract<std::string>(params, 0);
        auto config_str = glib2::Value::Extract<std::string>(params, 1);
        auto single_use = glib2::Value::Extract<bool>(params, 2);
        auto persistent = glib2::Value::Extract<bool>(params, 3);

        const std::string caller = args->GetCallerBusName();
        uid_t owner = creds_qry_->GetUID(caller);

        object_manager_->CreateObject<Configuration>(dbuscon_,
                                                     object_manager_,
                                                     creds_qry_,
                                                     sig_configmgr_event_,
                                                     config_path,
                                                     state_dir_,
                                                     name,
                                                     config_str,
                                                     single_use,
                                                     persistent,
                                                     owner,
                                                     signals_->GetLogLevel(),
                                                     logwr_);

        sig_configmgr_event_->Send(config_path, EventType::CFG_CREATED, owner);

        args->SetMethodReturn(glib2::Value::CreateTupleWrapped(config_path));
    }
    catch (const DBus::Exception &e)
    {
        throw DBus::Object::Method::Exception(e.GetRawError());
    }
    catch (const std::exception &e)
    {
        throw ConfigManager::Exception("Invalid configuration profile: "s
                                       + e.what());
    }
}


void ConfigHandler::method_fetch_available_configs(DBus::Object::Method::Arguments::Ptr args)
{
    auto configs = helper_retrieve_configs(args->GetCallerBusName(),
                                           [](Configuration::Ptr obj)
                                           {
                                               // We want all of them.
                                               return true;
                                           });

    std::vector<DBus::Object::Path> paths;

    for (auto &config : configs)
    {
        paths.push_back(config->GetPath());
    }

    args->SetMethodReturn(glib2::Value::CreateTupleWrapped(paths));
}


void ConfigHandler::method_lookup_config_name(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();

    auto config_name = glib2::Value::Extract<std::string>(params, 0);

    auto configs = helper_retrieve_configs(args->GetCallerBusName(),
                                           [config_name](Configuration::Ptr obj)
                                           {
                                               return obj->GetName() == config_name;
                                           });

    std::vector<DBus::Object::Path> paths;

    for (auto &config : configs)
    {
        paths.push_back(config->GetPath());
    }

    args->SetMethodReturn(glib2::Value::CreateTupleWrapped(paths));
}


void ConfigHandler::method_search_by_tag(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();

    auto tag = glib2::Value::Extract<std::string>(params, 0);

    auto configs = helper_retrieve_configs(args->GetCallerBusName(),
                                           [tag](Configuration::Ptr obj)
                                           {
                                               return obj->CheckForTag(tag);
                                           });

    std::vector<DBus::Object::Path> paths;

    for (auto &config : configs)
    {
        paths.push_back(config->GetPath());
    }

    args->SetMethodReturn(glib2::Value::CreateTupleWrapped(paths));
}


void ConfigHandler::method_search_by_owner(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();

    auto str_owner = glib2::Value::Extract<std::string>(params, 0);
    uid_t owner = get_userid(str_owner);

    auto configs = helper_retrieve_configs(args->GetCallerBusName(),
                                           [owner](Configuration::Ptr obj)
                                           {
                                               return obj->GetOwnerUID() == owner;
                                           });

    std::vector<DBus::Object::Path> paths;

    for (auto &config : configs)
    {
        paths.push_back(config->GetPath());
    }

    args->SetMethodReturn(glib2::Value::CreateTupleWrapped(paths));
}


void ConfigHandler::method_transfer_ownership(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();

    auto path = glib2::Value::Extract<DBus::Object::Path>(params, 0);
    uid_t new_owner_uid = glib2::Value::Extract<uid_t>(params, 0);

    auto configs = helper_retrieve_configs(args->GetCallerBusName(),
                                           [path](Configuration::Ptr obj)
                                           {
                                               return obj->GetPath() == path;
                                           });

    for (auto &config : configs)
    {
        config->TransferOwnership(new_owner_uid);
    }
}


ConfigHandler::ConfigCollection
ConfigHandler::helper_retrieve_configs(const std::string &caller,
                                       fn_search_filter &&filter_fn) const
{
    ConfigCollection configurations;

    for (const auto &[path, object] : object_manager_->GetAllObjects())
    {
        auto config_object = std::dynamic_pointer_cast<Configuration>(object);

        if (config_object)
        {
            // If the caller is empty, we don't do any ACL checks
            bool caller_check = ((!caller.empty() && config_object->CheckACL(caller))
                                 || caller.empty());

            // If the device object is not null, the path should be valid
            if (caller_check && filter_fn(config_object))
            {
                configurations.push_back(config_object);
            }
        }
    }

    return configurations;
}


// Service

Service::Service(DBus::Connection::Ptr con, LogWriter::Ptr lwr)
    : DBus::Service(con, SERVICE_CONFIGMGR),
      con_(con),
      logwr_(std::move(lwr))
{
    try
    {
        logsrvprx_ = LogServiceProxy::AttachInterface(con, INTERFACE_CONFIGMGR);
        config_handler_ = CreateServiceHandler<ConfigHandler>(con_,
                                                              GetObjectManager(),
                                                              loglevel_,
                                                              logwr_);
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
        logsrvprx_->Detach(INTERFACE_CONFIGMGR);
    }
}


void Service::BusNameAcquired(const std::string &busname)
{
}


void Service::BusNameLost(const std::string &busname)
{
    using namespace std::string_literals;

    throw DBus::Service::Exception("openvpn3-service-"s + SERVICE_ID + " lost the '"
                                   + busname + "' registration on the D-Bus");
}


void Service::SetLogLevel(uint8_t loglvl)
{
    loglevel_ = loglvl;
}

void Service::SetStateDirectory(const std::string &stdir)
{
    config_handler_->SetStateDirectory(stdir);
}


} // namespace ConfigManager
