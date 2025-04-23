//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2022 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2022 - 2023  David Sommerseth <davids@openvpn.net>
//
//

/**
 * @file   journal.cpp
 *
 * @brief  Admin command to retrieve OpenVPN 3 Linux log entries
 *         from the systemd-journald
 */

#include "build-config.h"

#include <cstdarg>
#include <filesystem>
#include <iostream>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/proxy.hpp>

#ifdef HAVE_SELINUX
#include <selinux/selinux.h>
#include <selinux/restorecon.h>
#endif

#include "common/cmdargparser.hpp"
#include "common/lookup.hpp"
#include "log/service-configfile.hpp"
#include "netcfg/netcfg-configfile.hpp"
#include "netcfg/dns/resolvconf-file.hpp"

#ifdef HAVE_SYSTEMD
#include "netcfg/dns/proxy-systemd-resolved.hpp"
#endif

namespace fs = std::filesystem;
using namespace NetCfg::DNS;

/**
 *  Settings provided via the command line to the init-config command
 *
 */
struct setup_config
{
    fs::path statedir = OPENVPN3_STATEDIR;
    bool write = false;
    bool overwrite = false;
    bool debug = false;
    uid_t openvpn_uid = -1;
    gid_t openvpn_gid = -1;
};



//
//
//  Configuration::File helper functions
//
//

/**
 *  Generic helper function to unset settings in a configuration file object.
 *  This function will not bail out if any exceptions happens.  This is needed to
 *  handle removal of settings which was not set in the configuration file.
 *
 * @param cfgfile  The Configuration::File based object to modify
 * @param opts     A std::string array of options to remove.
 */
void unset_configfile_options(Configuration::File &cfgfile, const std::vector<std::string> &opts)
{
    for (const auto &o : opts)
    {
        try
        {
            cfgfile.UnsetOption(o);
        }
        catch (...)
        {
            // Ignore errors here.  If the config file is empty, it will fail.
        }
    }
}


/**
 *  Generic helper function to save the configuration file to disk
 *
 *  This function will respect the write and overwrite flags as provided by the
 *  command line
 *
 * @param cfg       The Configuration::File based object to write to disk
 * @param cfgname   The filename, including path, where to save it
 * @param setupcfg  The command line option settings
 */
void save_config_file(Configuration::File &cfg, const fs::path cfgname, const setup_config &setupcfg)
{
    try
    {
        if (setupcfg.write && (!fs::exists(cfgname) || setupcfg.overwrite))
        {
            cfg.Save(cfgname);
            std::cout << "    Configuration saved" << std::endl;
        }
        else
        {
            if (setupcfg.write && fs::exists(cfgname))
            {
                std::cout << "    !! Will not overwrite existing configuration file" << std::endl;
            }
            std::cout << "    !! Configuration UNCHANGED" << std::endl;
        }

        if (!fs::exists(cfgname))
        {
            return;
        }

        if (0 != chmod(cfgname.c_str(), 0644))
        {
            std::cout << "    ** ERROR ** Failed to change the file access on the config file (chmod 0644)" << std::endl;
        }
        if (0 != chown(cfgname.c_str(), setupcfg.openvpn_uid, setupcfg.openvpn_gid))
        {
            std::cout << "    ** ERROR ** Failed to change the file ownership on the config file" << std::endl;
        }
    }
    catch (const fs::filesystem_error &err)
    {
        throw CommandException("init-config(save_config_file)", "Unexpected error saving config: " + err.code().message());
    }
}



//
//
//  Logger service
//
//

/**
 *  Log methods supported by openvpn3-service-log which are reasonable for defaults
 *
 */
enum class LogMethod
{
    NONE,    //  No logging configured; this should never be seen as the end result
    SYSLOG,  //  Prefer to use the traditional syslog() API
    JOURNALD //  Prefer to use systemd-journald
};


/**
 *  Detect if the systemd-journald is running on the system, which is then considered
 *  and evidence for that being available for system logging.  Otherwise fallback to syslog.
 *
 * @param setupcfg
 */
void configure_logger(const setup_config &setupcfg, DBus::Connection::Ptr dbuscon)
{
    LogMethod logm = LogMethod::NONE;

    std::cout << std::endl
              << "* Logger Configuration" << std::endl;

    const fs::path cfgfile = setupcfg.statedir / "log-service.json";
    std::cout << "    Configuration file: " << cfgfile << std::endl;

#ifdef HAVE_SYSTEMD
    try
    {
        logm = LogMethod::SYSLOG;

        auto prx = DBus::Proxy::Client::Create(dbuscon,
                                               "org.freedesktop.systemd1");
        auto journald_service_tgt = DBus::Proxy::TargetPreset::Create(
            "/org/freedesktop/systemd1/unit/systemd_2djournald_2eservice",
            "org.freedesktop.systemd1.Unit");

        std::string state = prx->GetProperty<std::string>(journald_service_tgt,
                                                          "ActiveState");
        std::cout << "    systemd-journald active state: " << state << std::endl;
        if ("active" == state)
        {
            logm = LogMethod::JOURNALD;
        }
    }
    catch (const DBus::Proxy::Exception &excp)
    {
        std::cout << "    !! Could not retrieve systemd-journald.service details" << std::endl;
        if (setupcfg.debug)
        {
            std::cout << "    [DBus::Proxy::Exception] "
                      << excp.what() << std::endl;
        }
    }
#else
    std::cout << "    Built without systemd support; no systemd-journald support available" << std::endl;
    logm = LogMethod::SYSLOG;
#endif

    // Prepare a new config file
    LogServiceConfigFile logcfg;

    // If config file already exists, parse it to preserve other settings
    if (fs::exists(cfgfile))
    {
        logcfg.Load(cfgfile);
    }

    // Reset potentially previous log settings
    unset_configfile_options(logcfg,
                             {"journald",
                              "syslog",
                              "syslog-facility",
                              "log-file",
                              "timestamp",
                              "no-logtag-prefix",
                              "service-log-dbus-details"});

    switch (logm)
    {
    case LogMethod::NONE:
        std::cout << "     ** NO LOG METHOD CONFIGURED **" << std::endl;
        break;

    case LogMethod::JOURNALD:
        std::cout << "    :: Result ::  Will use systemd journald for logging" << std::endl;
        logcfg.SetValue("journald", true);
        logcfg.SetValue("service-log-dbus-details", true);
        break;

    case LogMethod::SYSLOG:
        std::cout << "    :: Result ::  Will use syslog for logging" << std::endl;
        logcfg.SetValue("syslog", true);
        break;
    }

    save_config_file(logcfg, cfgfile, setupcfg);
}



//
//
//   Network Configuration
//
//

/**
 *   DNS resolver modes supported by openvpn3-service-netcfg
 */
enum class DNSresolver
{
    NONE,      // No DNS resolver integration configured
    RESOLVED,  // Use systemd-resolved
    RESOLVCONF // Use /etc/resolv.conf
};


/**
 *  Checks if the given file path is accessible by this process.
 *
 * @param file
 */
bool is_accessible(const fs::path &file)
{
    // We should probably replace this with std::filesystem code.
    // For now, this is copied-over legacy code.

    if (fs::exists(file) && !fs::is_empty(file))
    {
        fs::perms fileperm = fs::status(file).permissions();
        fs::perms perm = fileperm & (fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read);
        if (perm != fs::perms::none)
        {
            std::cout << "    Found accessible " << file << std::endl;
            return true;
        }
    }

    std::cout << "    !! Could not read " << file << std::endl;
    return false;
}


/**
 *  Checks if the given file contains the special 127.0.0.53 nameserver.
 *
 * @param conf_file
 */
bool is_systemd_resolved_configured(const char *conf_file)
{
    // Parse the resolv config file to see if systemd-resolved
    // is configured or not
    std::cout << "    Parsing " << conf_file << " ... ";
    auto r = ResolvConfFile::Create(conf_file);
    std::cout << "Done" << std::endl;

    for (const auto &ns : r->GetNameServers())
    {
        if ("127.0.0.53" == ns)
        {
            std::cout << "    Found systemd-resolved configured "
                      << "(" << ns << ") in " << conf_file << " "
                      << std::endl;
            return true;
        }
    }

    return false;
}


/**
 *  Try to find which Network Configuration settings would be reasonable defaults
 *  on this host.
 *
 *  Only DNS resolver settings are detected.  If the systemd-resolverd service is
 *  found running, that will be the preferred integration.  Otherwise it will
 *  use /etc/resolv.conf if that file can be found and read.
 *
 * @param setupcfg
 */
void configure_netcfg(const setup_config &setupcfg, DBus::Connection::Ptr dbuscon)
{
    const fs::path cfgfile = setupcfg.statedir / "netcfg.json";
    DNSresolver resolver = DNSresolver::NONE;

    std::cout << std::endl
              << "* Network Configuration" << std::endl;
    std::cout << "    Configuration file: " << cfgfile << std::endl;

    // Detect if systemd-resolved is available or not
#ifdef HAVE_SYSTEMD
    try
    {
        auto resolvd = resolved::Manager::Create(dbuscon);
        resolver = DNSresolver::RESOLVED;
        std::cout << "    Found systemd-resolved" << std::endl;
    }
    catch (const resolved::Exception &excp)
    {
        std::cout << "    !! Could not access systemd-resolved" << std::endl;
        if (setupcfg.debug)
        {
            std::cout << "    [resolved::Exception] "
                      << excp.what() << std::endl;
        }
    }
    catch (const DBus::Exception &excp)
    {
        std::cout << "    !! Could not connect to D-Bus" << std::endl;
        if (setupcfg.debug)
        {
            std::cout << "    [DBus::Exception] "
                      << excp.what() << std::endl;
        }
    }
#else
    std::cout << "    Built without systemd support; no systemd-resolved support available" << std::endl;
#endif

    // Check if /etc/resolv.conf is available
    if (is_accessible("/etc/resolv.conf"))
    {
        if (DNSresolver::RESOLVED == resolver
            && !is_systemd_resolved_configured("/etc/resolv.conf"))
        {
            if (!is_accessible("/run/systemd/resolve/stub-resolv.conf")
                || !is_systemd_resolved_configured("/run/systemd/resolve/stub-resolv.conf"))
            {
                resolver = DNSresolver::RESOLVCONF;
            }
        }

        if (DNSresolver::NONE == resolver)
        {
            resolver = DNSresolver::RESOLVCONF;
        }
    }

    // Prepare a new config file
    NetCfgConfigFile config;

    // If config file already exists, parse it to preserve other settings
    if (fs::exists(cfgfile))
    {
        config.Load(cfgfile);
    }

    // Reset potentially existing DNS resolver settings
    unset_configfile_options(config,
                             {"systemd-resolved",
                              "resolv-conf"});

    // Configure new DNS resolver
    switch (resolver)
    {
    case DNSresolver::NONE:
        std::cout << "    :: Result :: No DNS resolver integration will configured" << std::endl;
        break;

    case DNSresolver::RESOLVED:
        std::cout << "    :: Result :: Will use systemd-resolved" << std::endl;
        config.SetValue("systemd-resolved", true);
        break;

    case DNSresolver::RESOLVCONF:
        std::cout << "    :: Result :: Will use /etc/resolv.conf" << std::endl;
        config.SetValue("resolv-conf", std::string("/etc/resolv.conf"));
        break;
    }

    save_config_file(config, cfgfile, setupcfg);
}



//
//
//   Generic information and state-dir related helper functions
//
//

/**
 *  Lookup the UID and GID values for the 'openvpn' username and group.
 *
 *  The values extracted will be used later on to set the proper ownership of
 *  configuration files
 *
 * @param setupcfg
 */
void get_openvpn_uid_gid(setup_config &setupcfg)
{
    // Check if the 'openvpn' user and group exists
    std::cout << std::endl
              << "* Checking for OpenVPN user and group accounts" << std::endl;
    try
    {
        setupcfg.openvpn_uid = lookup_uid(OPENVPN_USERNAME);
        std::cout << "    Found:  openvpn user - uid " << std::to_string(setupcfg.openvpn_uid) << std::endl;

        setupcfg.openvpn_gid = lookup_gid(OPENVPN_GROUP);
        std::cout << "    Found:  openvpn group - gid " << std::to_string(setupcfg.openvpn_gid) << std::endl;
    }
    catch (const LookupException &err)
    {
        throw CommandException("init-seutp", err.what());
    }
}



/**
 *  Check for the presence of the state/configuration directory used by OpenVPN 3 Linux
 *
 *  This will also enforce the openvpn user/group to own this directory and the ACL (chmod)
 *  defined as only that user/group can access it.
 *
 *  If this directory is missing, create it.
 *
 * @param setupcfg
 */
void setup_state_dir(const setup_config &setupcfg)
{
    std::cout << std::endl
              << "* Checking OpenVPN 3 Linux state/configuration directory" << std::endl
              << "    Using directory: " << setupcfg.statedir << std::endl;

    try
    {
        const fs::perms dirperms = (fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec);
        if (!fs::is_directory(setupcfg.statedir))
        {
            std::cout << "    -- State directory is missing - creating it" << std::endl;
            fs::create_directory(setupcfg.statedir);
        }
        else
        {
            std::cout << "    Directory found" << std::endl;
        }

        // Ensure the root state directory has proper ownership and permissions
        fs::permissions(setupcfg.statedir, dirperms, fs::perm_options::replace);
        if (0 != ::chown(setupcfg.statedir.c_str(), setupcfg.openvpn_uid, setupcfg.openvpn_gid))
        {
            throw CommandException("init-config", "Failed to change ownership of state directory");
        }

        // Check the configs sub-directory
        fs::path cfgdir = setupcfg.statedir / "configs";
        if (!fs::is_directory(cfgdir))
        {
            std::cout << "    -- Configurations sub-directory is missing - creating it" << std::endl;
            fs::create_directory(cfgdir);
        }
        else
        {
            std::cout << "    Configs sub-directory found" << std::endl;
        }

        // Ensure the configs sub-directory has proper ownership and permissions
        fs::permissions(cfgdir, dirperms, fs::perm_options::replace);
        if (0 != ::chown(cfgdir.c_str(), setupcfg.openvpn_uid, setupcfg.openvpn_gid))
        {
            throw CommandException("init-config", "Failed to change ownership of the configurations sub-directory");
        }
    }
    catch (const fs::filesystem_error &err)
    {
        std::ostringstream msg;
        msg << "Failed to prepare directory '" << err.path1() << "': "
            << err.code().message();
        throw CommandException("init-config", msg.str());
    }
}


/**
 *   Log callback for the SELinux restorecon call
 *
 */
int selinux_log_callback(int type, const char *fmt, ...)
{
#ifdef HAVE_SELINUX
    va_list ap;
    int len = 0;

    va_start(ap, fmt);
    len = vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);

    if (0 > len)
    {
        return 1;
    }
    ++len; // Add space for NUL terminator

    va_start(ap, fmt);
    char *p = (char *)::malloc(len);
    len = vsnprintf(p, len, fmt, ap);
    va_end(ap);

    std::string msg;
    if (0 > len)
    {
        free(p);
        msg = "[UNKNOWN]";
    }
    else
    {
        msg = std::string(p);
    }
    free(p);

    switch (type)
    {
    case SELINUX_ERROR:
        std::cout << "    ** ERROR ** " << msg;
        break;

    case SELINUX_WARNING:
        std::cout << "    !! " << msg;
        break;

    case SELINUX_INFO:
        std::cout << "    - " << msg;
        break;

    case SELINUX_AVC:
        break;
    }

#endif
    return 0;
}


/**
 *  This is only run if SELinux is available.  It will ensure the files and directories
 *  in the state/configuration directory has the proper SELinux contexts
 *
 * @param setupcfg
 */
void selinux_restorecon(const setup_config &setupcfg)
{
#ifdef HAVE_SELINUX
    union selinux_callback cb;
    cb.func_log = selinux_log_callback;
    selinux_set_callback(SELINUX_CB_LOG, cb);

    std::cout << std::endl
              << "* Ensuring SELinux file labels are correct" << std::endl;

    int s;
    if (selinux_getenforcemode(&s) != 0)
    {
        std::cout << "    - Could not retrieve SELinux status; skipping" << std::endl;
        return;
    }
    switch (s)
    {
    case -1:
        std::cout << "    - SELinux status: Not enabled; skipping" << std::endl;
        return;

    case 0:
        std::cout << "    - SElinux status: Permissive mode" << std::endl;
        break;

    case 1:
        std::cout << "    - SELinux status: Enforcing mode" << std::endl;
        break;

    default:
        std::cout << "    - SELinux status: Unknown; skipping" << std::endl;
        return;
    }

    int r = selinux_restorecon(setupcfg.statedir.c_str(),
                               SELINUX_RESTORECON_RECURSE
                                   | SELINUX_RESTORECON_IGNORE_DIGEST
                                   | SELINUX_RESTORECON_VERBOSE);
    if (0 != r)
    {
        std::cout << "    ** Failed restoring SELinux file contexts" << std::endl;
    }
#endif
}



/**
 *   Main function for 'openvpn3 init-config'
 *
 * @param args Pointer to a ParsedArgs object with all pre-parsed command line options
 * @return int  Exit code of the command
 */
int cmd_initcfg(ParsedArgs::Ptr args)
{
    if (::geteuid() != 0)
    {
        throw CommandException("init-config", "Must be run as root");
    }

    struct setup_config setupcfg;
    if (args->Present("state-dir"))
    {
        setupcfg.statedir = args->GetLastValue("state-dir");
    }
    setupcfg.write = args->Present("write-configs");
    setupcfg.overwrite = args->Present("force");
    setupcfg.debug = args->Present("debug");

    // Basic information
    std::cout << "- Detected settings will be saved to disk? "
              << (setupcfg.write ? "YES" : "No") << std::endl;
    if (setupcfg.write)
    {
        std::cout << "- Existing configurations will be "
                  << (setupcfg.overwrite ? "OVERWRITTEN" : "preserved") << std::endl;
    }

    auto dbusconn = DBus::Connection::Create(DBus::BusType::SYSTEM);

    // Find the proper UID and GID values for the openvpn user and group
    get_openvpn_uid_gid(setupcfg);

    // Ensure the state directory exists and has proper privileges
    setup_state_dir(setupcfg);

    // Configure the logger service
    configure_logger(setupcfg, dbusconn);

    // Detect and configure settings suitable for the NetCfg service
    configure_netcfg(setupcfg, dbusconn);

    // Ensure SELinux file contexts are correctly setup
    selinux_restorecon(setupcfg);

    return 0;
}



/**
 *  Creates the SingleCommand object for the 'journal' admin command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_initcfg()
{
    SingleCommand::Ptr initcfg;
    initcfg.reset(new SingleCommand("init-config",
                                    "Automated configuration tool",
                                    cmd_initcfg));
    initcfg->AddComment(SingleCommand::CommentPlacement::BEFORE_OPTS,
                        "  This command is used to auto-detect a few configuration settings");
    initcfg->AddComment(SingleCommand::CommentPlacement::BEFORE_OPTS,
                        "  needed for OpenVPN 3 Linux to run properly.");
    initcfg->AddComment(SingleCommand::CommentPlacement::AFTER_OPTS,
                        "  **  This command requires root privileges to run");
    initcfg->AddOption("write-configs", "Write configuration files to disk");
    initcfg->AddOption("force", "Force overwriting existing configuration files");
    initcfg->AddOption("state-dir", "DIRNAME", true, "Main OpenVPN 3 Linux state directory (Default: '" OPENVPN3_STATEDIR "')");
    initcfg->AddOption("debug", "Print debug details in error situtations");
    return initcfg;
}
