//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2022         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2022         David Sommerseth <davids@openvpn.net>
//
//

/**
 * @file   journal.cpp
 *
 * @brief  Admin command to retrieve OpenVPN 3 Linux log entries
 *         from the systemd-journald
 */

#include "config.h"

#include <iostream>
#include <cstdarg>
#include <sys/stat.h>

#ifdef HAVE_LIBSELINUX
#include <selinux/selinux.h>
#include <selinux/restorecon.h>
#endif

#include "openvpn/common/stat.hpp"

#include "dbus/proxy.hpp"
#include "common/cmdargparser.hpp"
#include "common/lookup.hpp"
#include "log/service-configfile.hpp"
#include "netcfg/netcfg-configfile.hpp"

#ifdef HAVE_SYSTEMD
#include "netcfg/dns/proxy-systemd-resolved.hpp"
using namespace NetCfg::DNS;
#endif


/**
 *  Settings provided via the command line to the init-config command
 *
 */
struct setup_config
{
    std::string statedir = OPENVPN3_STATEDIR;
    bool write = false;
    bool overwrite = false;
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
void save_config_file(Configuration::File &cfg, const std::string cfgname, const setup_config &setupcfg)
{
    if (setupcfg.write && (!openvpn::file_exists(cfgname) || setupcfg.overwrite))
    {
        cfg.Save(cfgname);
        std::cout << "    Configuration saved" << std::endl;
    }
    else
    {
        if (setupcfg.write && openvpn::file_exists(cfgname))
        {
            std::cout << "    !! Will not overwrite existing configuration file" << std::endl;
        }
        std::cout << "    !! Configuration file UNCHANGED" << std::endl;
    }
    chmod(cfgname.c_str(), 0644);
    chown(cfgname.c_str(), setupcfg.openvpn_uid, setupcfg.openvpn_gid);
}



//
//
//  Logger service
//
//

/**
 *  Log methods supported by openvpn3-service-logger which are reasonable for defaults
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
void configure_logger(const setup_config &setupcfg)
{
    LogMethod logm = LogMethod::NONE;

    std::cout << std::endl
              << "* Logger Configuration" << std::endl;

    const std::string cfgfile = setupcfg.statedir + "/log-service.json";
    std::cout << "    Configuration file: " << cfgfile << std::endl;

#ifdef HAVE_SYSTEMD
    try
    {
        logm = LogMethod::SYSLOG;

        DBusProxy prx(G_BUS_TYPE_SYSTEM,
                      "org.freedesktop.systemd1",
                      "org.freedesktop.systemd1.Unit",
                      "/org/freedesktop/systemd1/unit/systemd_2djournald_2eservice");
        std::string state = prx.GetStringProperty("ActiveState");
        std::cout << "    systemd-journald active state: " << state << std::endl;
        if ("active" == state)
        {
            logm = LogMethod::JOURNALD;
        }
    }
    catch (const DBusException &excp)
    {
        std::cout << "    !! Could not retrieve systemd-journald.service details" << std::endl;
        std::cerr << excp.what() << std::endl;
    }
#else
    std::cout << "    Built without systemd support; no systemd-journald support available" << std::endl;
    logm = LogMethod::SYSLOG;
#endif

    // Prepare a new config file
    LogServiceConfigFile logcfg;

    // If config file already exists, parse it to preserve other settings
    if (openvpn::file_exists(cfgfile))
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
 *  Try to find which Network Configuration settings would be reasonable defaults
 *  on this host.
 *
 *  Only DNS resolver settings are detected.  If the systemd-resolverd service is
 *  found running, that will be the preferred integration.  Otherwise it will
 *  use /etc/resolv.conf if that file can be found and read.
 *
 * @param setupcfg
 */
void configure_netcfg(const setup_config &setupcfg)
{
    const std::string cfgfile = setupcfg.statedir + "/netcfg.json";
    DNSresolver resolver = DNSresolver::NONE;

    std::cout << std::endl
              << "* Network Configuration" << std::endl;
    std::cout << "    Configuration file: " << cfgfile << std::endl;

    // Detect if systemd-resolved is available or not
#ifdef HAVE_SYSTEMD
    try
    {
        DBus conn(G_BUS_TYPE_SYSTEM);
        conn.Connect();
        resolved::Manager resolvd(conn.GetConnection());
        resolver = DNSresolver::RESOLVED;
        std::cout << "    Found systemd-resolved" << std::endl;
    }
    catch (const resolved::Exception &excp)
    {
        std::cout << "    !! Could not access systemd-resolved" << std::endl;
    }
    catch (const DBusException &excp)
    {
        std::cout << "    !! Could not connect to D-Bus" << std::endl;
    }
#else
    std::cout << "    Built without systemd support; no systemd-resolved support available" << std::endl;
#endif

    // Check if /etc/resolv.conf is available
    {
        struct stat st;
        if (::stat("/etc/resolv.conf", &st) == 0)
        {
            if ((st.st_size > 0)
                && ((st.st_mode & (S_IRUSR | S_IWUSR)) > 0))
            {
                if (DNSresolver::NONE == resolver)
                {
                    resolver = DNSresolver::RESOLVCONF;
                }
                std::cout << "    Found accessible /etc/resolv.conf" << std::endl;
            }
            else
            {
                std::cout << "    !! Could not read /etc/resolv.conf" << std::endl;
            }
        }
    }

    // Prepare a new config file
    NetCfgConfigFile config;

    // If config file already exists, parse it to preserve other settings
    if (openvpn::file_exists(cfgfile))
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

    if (!is_directory(setupcfg.statedir, true))
    {
        std::cout << "    -- Configuration is missing - creating it" << std::endl;
        if (0 != ::mkdir(setupcfg.statedir.c_str(), S_IRWXU | S_IRGRP | S_IXGRP))
        {
            throw CommandException("init-setup", "Failed to create a new state/configuration directory");
        }
    }
    else
    {
        std::cout << "    Directory found" << std::endl;
        if (0 != ::chmod(setupcfg.statedir.c_str(), S_IRWXU | S_IRGRP | S_IXGRP))
        {
            throw CommandException("init-setup", "Failed fixing access mode to state/configuration directory (chmod)");
        }
    }

    if (0 != ::chown(setupcfg.statedir.c_str(), setupcfg.openvpn_uid, setupcfg.openvpn_gid))
    {
        throw CommandException("init-setup", "Failed to change ownership of state/configuration directory");
    }
}


/**
 *   Log callback for the SELinux restorecon call
 *
 */
int selinux_log_callback(int type, const char *fmt, ...)
{
#ifdef HAVE_LIBSELINUX
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
#ifdef HAVE_LIBSELINUX
    union selinux_callback cb;
    cb.func_log = selinux_log_callback;
    selinux_set_callback(SELINUX_CB_LOG, cb);

    std::cout << std::endl
              << "* Ensuring SELinux file labels are correct" << std::endl;

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
 *   Main function for 'openvpn3 init-setup'
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

    // Basic information
    std::cout << "- Detected settings will be saved to disk? "
              << (setupcfg.write ? "YES" : "No") << std::endl;
    if (setupcfg.write)
    {
        std::cout << "- Existing configurations will be "
                  << (setupcfg.overwrite ? "OVERWRITTEN" : "preserved") << std::endl;
    }

    // Find the proper UID and GID values for the openvpn user and group
    get_openvpn_uid_gid(setupcfg);

    // Ensure the state directory exists and has proper privileges
    setup_state_dir(setupcfg);

    // Configure the logger service
    configure_logger(setupcfg);

    // Detect and configure settings suitable for the NetCfg service
    configure_netcfg(setupcfg);

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
    return initcfg;
}
