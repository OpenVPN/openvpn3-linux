//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2020  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2020  David Sommerseth <davids@openvpn.net>
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
 * @file   resolvconf-file.cpp
 *
 * @brief  This will update resolv.conf files directly, in the typical
 *         resolv.conf format.  Normally, this would be /etc/resolv.conf, but
 *         can be used for other files as well, if there is a local
 *         resolver service which picks these files some elsewhere.
 */


#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <sys/stat.h>

#include <openvpn/common/rc.hpp>

#include "common/timestamp.hpp"
#include "netcfg/dns/resolver-settings.hpp"
#include "netcfg/dns/resolver-backend-interface.hpp"
#include "netcfg/dns/resolvconf-file.hpp"
#include "netcfg/netcfg-exception.hpp"

using namespace NetCfg::DNS;

//
//  NetCfg::DNS::FileGenerator
//

FileGenerator::FileGenerator(const std::string& filename,
                             const std::string& backup_filename)
        : filename(filename), backup_filename(backup_filename)
{
}


FileGenerator::~FileGenerator()
{
}


void FileGenerator::SetFilename(const std::string& fname) noexcept
{
    filename = fname;
}


const std::string FileGenerator::GetFilename() const noexcept
{
    return filename;
}


void FileGenerator::SetBackupFilename(const std::string& bfname) noexcept
{
    backup_filename = bfname;
}


const std::string FileGenerator::GetBackupFilename() const noexcept
{
    return backup_filename;
}


void FileGenerator::Read()
{
    std::ifstream input(filename);
    file_contents.clear();
    for (std::string line; std::getline(input, line);)
    {
        file_contents.push_back(line);
    }
    input.close();
}


/**
 *  Writes the contents of the protected: file_contents to disk.
 *  If a backup file is requested, ensure that is arranged before
 *  anything else is done.  If a backupfile already exists with
 *  the same filename, the old backup file is automatically removed.
 *
 */
void FileGenerator::Write()
{
    if (filename == backup_filename)
    {
        throw NetCfgException("The filename and backup filename "
                              "cannot be identical");
    }

    // If we're going to take a backup if the file exists
    if (!backup_filename.empty() && !backup_active
        && file_exists(filename))
    {
        if (file_exists(backup_filename))
        {
            std::remove(backup_filename.c_str());
        }
        if (0 != std::rename(filename.c_str(), backup_filename.c_str()))
        {
            throw NetCfgException("Could not rename '" + filename +"'"
                                  + " to '" + backup_filename + "'");
        }
        backup_active = true;
    }

    // Open the destination file, generate contents and write to disk
    std::fstream output(filename, output.out);
    for (const auto& line : file_contents)
    {
        output << line << std::endl;;
    }
}


/**
 *  If a file was overwritten and a backup was made, this will
 *  restore the backup file to the provided filename
 *
 */
void FileGenerator::RestoreBackup()
{
    if (!backup_active)
    {
        // No backup file has been created, so nothing to restore.
        return;
    }
    if (backup_filename.empty() || !file_exists(backup_filename))
    {
        throw NetCfgException("Backup file '" + backup_filename + "'"
                              + " to restore '" + filename + "'"
                              + " is missing");
    }

    if (0 != std::remove(filename.c_str()))
    {
        throw NetCfgException("Failed removing '" + filename + "'");
    }

    if (0 != std::rename(backup_filename.c_str(), filename.c_str()))
    {
        throw NetCfgException("Failed restoring '" + filename + "'"
                              + " from '" + backup_filename + "'");
    }
    backup_active = false;
}


/**
 *  Simple method to check if a file already exists.  It does
 *  not do any permissions or availability checks, only if a file
 *  of some kind exists.
 *
 * @param fname  std::string of filename to look for
 * @return  Returns true if file exists, otherwise false.
 */
bool FileGenerator::file_exists (const std::string& fname) noexcept
{
    struct stat buffer;
    return (stat (fname.c_str(), &buffer) == 0);
}


//
//  NetCfg::DNS::ResolvConfFile
//

ResolvConfFile::ResolvConfFile(const std::string& filename,
               const std::string& backup_filename)
        : FileGenerator(filename, backup_filename)
{
}


ResolvConfFile::~ResolvConfFile()
{
    RestoreBackup();
}


const ApplySettingsMode ResolvConfFile::GetApplyMode() const noexcept
{
    return ApplySettingsMode::MODE_PRE;
}


const std::string ResolvConfFile::GetBackendInfo() const noexcept
{
    std::string ret = std::string("ResolvConf file backend. Using: ")
                    + "'" + GetFilename() + "'";
    if (!GetBackupFilename().empty())
    {
        ret += "  Backup file: '" + GetBackupFilename() + "'";
    }
    return ret;
}


void ResolvConfFile::Apply(const ResolverSettings::Ptr settings)
{
    // Copy all DNS servers and search domains to our own variables.
    //
    // Since Apply() can be called more times, we just extend the
    // current lists we have
    //
    //  We also need to consider if the ResolverSettings object is
    //  enabled or disabled.  If it is disabled, we need to ensure
    //  we remove those settings and does not confuse it as system
    //  settings when parsing the resolv.conf file later on.

    std::vector<std::string> srvs = settings->GetNameServers();
    std::vector<std::string> dmns = settings->GetSearchDomains();
    if (settings->GetEnabled())
    {
        vpn_name_servers.insert(vpn_name_servers.end(),
                                srvs.begin(), srvs.end());
        for (const auto& e : srvs)
        {
            NetCfgChangeEvent ev(NetCfgChangeType::DNS_SERVER_ADDED,
                                 "", {{"dns_server", e}});
            notification_queue.push_back(ev);
        }

        vpn_search_domains.insert(vpn_search_domains.end(),
                                  dmns.begin(), dmns.end());
        for (const auto& e : dmns)
        {
            NetCfgChangeEvent ev(NetCfgChangeType::DNS_SEARCH_ADDED,
                                 "", {{"search_domain", e}});
            notification_queue.push_back(ev);
        }
    }
    else
    {
        vpn_name_servers_removed.insert(vpn_name_servers_removed.end(),
                                        srvs.begin(), srvs.end());
        for (const auto& e : srvs)
        {
            NetCfgChangeEvent ev(NetCfgChangeType::DNS_SERVER_REMOVED,
                                 "", {{"dns_server", e}});
            notification_queue.push_back(ev);
        }

        vpn_search_domains_removed.insert(vpn_search_domains_removed.end(),
                                          dmns.begin(), dmns.end());
        for (const auto& e : dmns)
        {
            NetCfgChangeEvent ev(NetCfgChangeType::DNS_SEARCH_REMOVED,
                                 "", {{"search_domain", e}});
            notification_queue.push_back(ev);
        }
    }

    // Prepare to add a warning to the logs if DNS scope is not GLOBAL
    // resolv.conf based resolver does not support any other modes
    dns_scope_non_global = settings->GetDNSScope() != DNS::Scope::GLOBAL;

    ++modified_count;
}


void ResolvConfFile::Commit(NetCfgSignals *signal)
{
    // Add a lock guard here, to avoid potentially multiple calls colliding
    std::lock_guard<std::mutex> guard(change_guard);

    // Read and parse the current resolv.conf file
    Read();
    parse();

    // Generate the new file and write it to disk
    // if DNS resolver configs from VPN sessions
    // needs to be applied
    if (modified_count > 0)
    {
        generate();
        Write();
    }
    else
    {
        // If no changes is needed, restore the
        // original resolv.conf file
        RestoreBackup();

        // Since we have restored the original
        // resolv.conf, we need to re-parse it
        // on the next Commit() - which means
        // we are not interested in current
        // parsed system settings
        sys_name_servers.clear();
        sys_search_domains.clear();
    }

    // Send all NetworkChange events in the notification queue
    if (signal)
    {
        if (dns_scope_non_global)
        {
            signal->LogWarn("DNS Scope change ignored. Only global scope supported");
        }

        for (const auto& ev : notification_queue)
        {
            signal->NetworkChange(ev);
        }
    }
    notification_queue.clear();

    modified_count = 0;
}


void ResolvConfFile::Restore()
{
    RestoreBackup();
}

#ifdef ENABLE_DEBUG
/**
 *  Do a forceful read of the resolv.conf file and parse it
 */
void ResolvConfFile::Debug_Fetch()
{
    Read();
    parse();
}

void ResolvConfFile::Debug_Write()
{
    generate();
    FileGenerator::Write();
}

std::vector<std::string> ResolvConfFile::Debug_Get_dns_servers()
{
    std::vector<std::string> ret;
    ret = vpn_name_servers;
    ret.insert(ret.end(),
               sys_name_servers.begin(), sys_name_servers.end());
    return ret;
}

std::vector<std::string> ResolvConfFile::Debug_Get_search_domains()
{
    std::vector<std::string> ret;
    ret = vpn_search_domains;
    ret.insert(ret.end(),
               sys_search_domains.begin(), sys_search_domains.end());
    return ret;
}


std::string ResolvConfFile::Dump()
{
    std::stringstream ret;

    int i = 0;
    for (const auto& e : vpn_search_domains)
    {
        ret << "vpn_search_domains  [" << i << "]: " << e << std::endl;
        ++i;
    }

    i = 0;
    for (const auto& e : vpn_search_domains_removed)
    {
        ret << "vpn_search_domains_removed  [" << i << "]: " << e << std::endl;
        ++i;
    }

    i = 0;
    for (const auto& e : sys_search_domains)
    {
        ret << "sys_search_domains  [" << i << "]: " << e << std::endl;
        ++i;
    }

    i = 0;
    for (const auto& e : vpn_name_servers)
    {
        ret << "vpn_dns_servers [" << i << "]: " << e << std::endl;
        ++i;
    }

    i = 0;
    for (const auto& e : vpn_name_servers_removed)
    {
        ret << "vpn_dns_servers_removed [" << i << "]: " << e << std::endl;
        ++i;
    }

    i = 0;
    for (const auto& e : sys_name_servers)
    {
        ret << "sys_dns_servers [" << i << "]: " << e << std::endl;
        ++i;
    }
    return ret.str();
}
#endif


void ResolvConfFile::parse()
{
    std::vector<std::string> rslv_servers;
    std::vector<std::string> rslv_search;
    unprocessed_lines.clear();

    // Parse the currently active resolv.conf settings
    for (const auto& line : file_contents)
    {
        if (line.find("nameserver ") == 0)
        {
            rslv_servers.push_back(line.substr(11));
        }
        else if (line.find("search ") == 0)
        {
            size_t idx = 0;
            do
            {
                idx = line.find(" ", idx+1);
                if (std::string::npos != idx)
                {
                    rslv_search.push_back(line.substr(idx+1, line.substr(idx+1).find(" ")));
                }
            } while( std::string::npos != idx );
        }
        else if (line.find("#") == 0 || line.size() < 1)
        {
            // We ignore comment and empty lines
        }
        else
        {
            // Put aside all other lines, which will be written back
            // to the file when Write() called
            unprocessed_lines.push_back(line);
        }
    }

    // Apply unique records which has not been set
    //
    // This parse() method is only to be called from the Commit()
    // method, which means all Apply() calls has been completed for
    // all running VPN sessions.
    //
    // This means we can consider the content of the resolv.conf file
    // not found in "local" settings (from VPN sessions) to be
    // considered system settings
    //
    for (const auto& srv : rslv_servers)
    {
        auto sys = std::find(sys_name_servers.begin(),
                             sys_name_servers.end(),
                             srv);
        auto local = std::find(vpn_name_servers.begin(),
                               vpn_name_servers.end(),
                               srv);
        auto removed = std::find(vpn_name_servers_removed.begin(),
                                 vpn_name_servers_removed.end(),
                                 srv);
        if (sys == std::end(sys_name_servers)
            && local == std::end(vpn_name_servers)
            && removed == std::end(vpn_name_servers_removed))
        {
            //  This is a unique record not already seen
            sys_name_servers.push_back(srv);
        }
    }

    for (const auto& srch : rslv_search)
    {
        auto sys = std::find(sys_search_domains.begin(),
                             sys_search_domains.end(),
                             srch);
        auto local = std::find(vpn_search_domains.begin(),
                               vpn_search_domains.end(),
                               srch);
        auto removed = std::find(vpn_search_domains_removed.begin(),
                                 vpn_search_domains_removed.end(),
                                 srch);
        if (sys == std::end(sys_search_domains)
            && local == std::end(vpn_search_domains)
            && removed == std::end(vpn_search_domains_removed))
        {
            sys_search_domains.push_back(srch);
        }
    }
}


/**
 *  Generates the contents needed for a file write (@Write())
 *  which uses the resolv.conf format.  It
 */
void ResolvConfFile::generate()
{
    file_contents.clear();

    file_contents.push_back("#");
    file_contents.push_back("# Generated by OpenVPN 3 Linux (NetCfg::DNS::ResolvConfFile)");
    file_contents.push_back("# Last updated: " + GetTimestamp());
    file_contents.push_back("#");

    //
    //  'search <list-of-domains>' line
    //
    std::stringstream dns_search_line;
    for (const auto& e : vpn_search_domains)
    {
        dns_search_line << " " << e;
    }
    for (const auto& e : sys_search_domains)
    {
        dns_search_line << " " << e;
    }
    if (!dns_search_line.str().empty())
    {
        file_contents.push_back("search" + dns_search_line.str());
    }

    //
    //  'nameserver' lines, first VPN related ones then system
    //
    if (vpn_name_servers.size() > 0)
    {
        file_contents.push_back("");
        file_contents.push_back("# OpenVPN defined name servers");
        for (const auto& e : vpn_name_servers)
        {
            file_contents.push_back("nameserver " + e);
        }
    }

    if (sys_name_servers.size() > 0)
    {
        file_contents.push_back("");
        file_contents.push_back("# System defined name servers");
        for (const auto& e : sys_name_servers)
        {
            file_contents.push_back("nameserver " + e);
        }
    }

    //
    //  Finally all the lines of various settings
    //  not touched by us.
    //
    if (unprocessed_lines.size() > 0)
    {
        file_contents.push_back("");
        file_contents.push_back("# Other system settings");
        for (const auto& e : unprocessed_lines)
        {
            file_contents.push_back(e);
        }
    }

    // Clear all the VPN session settings, as the Apply()
    // method will be called again before the next update by the
    // DNS SettingsManager
    vpn_name_servers.clear();
    vpn_search_domains.clear();
    unprocessed_lines.clear();
}
