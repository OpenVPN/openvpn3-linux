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
 * @file   resolvconf-file.hpp
 *
 * @brief  This will update resolv.conf files directly, in the typical
 *         resolv.conf format.  Normally, this would be /etc/resolv.conf, but
 *         can be used for other files as well, if there is a local
 *         resolver service which picks these files some elsewhere.
 */


#pragma once

#include <mutex>
#include <string>
#include <vector>

#include <openvpn/common/rc.hpp>

using namespace openvpn;

#include "netcfg/netcfg-signals.hpp"
#include "netcfg/dns/resolver-settings.hpp"
#include "netcfg/dns/resolver-backend-interface.hpp"


namespace NetCfg
{
namespace DNS
{
    /**
     *  Simple generic file generator and parser
     *
     *  This can load and save files, take backup of files being overwritten
     *  and restore overwritten files.
     *
     *  The parsing and contents generating needs to happen in classes
     *  implementing this class.  The @generate() and @parse() methods needs
     *  to be implemented for this class to work.
     *
     */
    class FileGenerator
    {
    public:
        /**
         *  Construct the FileGenerator
         *
         * @param filename        std::string of the filename this object is
         *                        tied to.
         * @param backup_filename std::string.  If non-empty and a backup file
         *                        will be created if the Write() operation
         *                        will overwrite an existing file.
         */
        FileGenerator(const std::string& filename,
                      const std::string& backup_filename = "");
        virtual ~FileGenerator();

        /**
         *  Changes the filename of the resolv.conf file to process
         *  after initialization has completed
         *
         * @param fname  std::string containing the new filename
         */
        virtual void SetFilename(const std::string& fname) noexcept;

        /**
         *  Retrieve the filename currently in use
         *
         * @return  Returns a std::string containing the filename
         */
        virtual const std::string GetFilename() const noexcept;


        /**
         *  Change the filename used for the backup file after initialization
         *
         * @param bfname  std::string containing the new filename of the
         *                backup file
         */
        virtual void SetBackupFilename(const std::string& bfname) noexcept;


        /**
         *  Retrieve the filename currently in use for the backup file
         *
         * @return  Returns a std::string containing the filename
         */
        virtual const std::string GetBackupFilename() const noexcept;


    protected:
        std::vector<std::string> file_contents;


        /**
         *  Reads the file and saves the contents into the
         *  protected: files_content variable
         */
        void Read();


        /**
         *  Writes the contents of the protected: file_contents to disk.
         *  If a backup file is requested, ensure that is arranged before
         *  anything else is done.  If a backupfile already exists with
         *  the same filename, the old backup file is automatically removed.
         *
         */
        void Write();


        /**
         *  If a file was overwritten and a backup was made, this will
         *  restore the backup file to the currently configured main filename
         */
        void RestoreBackup();


    private:
        std::string filename = "";
        std::string backup_filename= "";
        bool backup_active = false;


        /**
         *  Simple method to check if a file already exists.  It does
         *  not do any permissions or availability checks, only if a file
         *  of some kind exists.
         *
         * @param fname  std::string of filename to look for
         * @return  Returns true if file exists, otherwise false.
         */
        bool file_exists (const std::string& fname) noexcept;
    };




    /**
     *  Class able to read, parse, manipulate and write resolv.conf files.
     *  This builds on the @FileGenerator and @ResolverSettings classes
     */
    class ResolvConfFile : public FileGenerator,
                           public ResolverBackendInterface
    {
    public:
        typedef RCPtr<ResolvConfFile> Ptr;

        /**
         *  Instantiate a resolv.conf parser
         *
         * @param filename  std::string of full path to the resolv.conf
         *                  file to parse
         * @param backup    bool, if a backup should be made automatically
         *                  on overwrites (default: false)
         */
        ResolvConfFile(const std::string& filename,
                       const std::string& backup_filename = "");


        ~ResolvConfFile();


        /**
         *  Retrieve information about the configured resolver backend.
         *  Used for log events
         *
         * @return Returns a constant std::string with backend details.
         */
        const std::string GetBackendInfo() const noexcept override;

        /**
         *  Retrieve when the backend can apply the DNS resolver settings.
         *  Normally it is applied before the tun interface configuration,
         *  but some backends may need information about the device to
         *  complete the configuration.
         *
         *  For the ResolvConfFile implementaion, this will always return
         *  MODE_PRE, where resolver settings will be applied before the
         *  tun interface is configured.
         *
         * @returns NetCfg::DNS:ApplySettingsMode
         */
        const ApplySettingsMode GetApplyMode() const noexcept override;

        /**
         *  Add new DNS resolver settings.  This may be called multiple times
         *  in the order it will be processed by the resolver backend.
         *
         * @param settings  A ResolverSettings::Ptr object containing the
         *                  DNS resolver configuraiton data
         */
        void Apply(const ResolverSettings::Ptr settings) override;

        /**
         *  Completes the DNS resolver configuration by performing the
         *  changes on the system.
         *
         *  @param signal  Pointer to a NetCfgSignals object where
         *                 "NetworkChange" signals will be issued
         */
        void Commit(NetCfgSignals *signal) override;


        /**
         *  Restore the backup resolv.conf file if present
         */
        void Restore();


#ifdef ENABLE_DEBUG
        /**
         *  Misc debug methods used by test programs
         */
        void Debug_Fetch();
        void Debug_Write();

        std::vector<std::string> Debug_Get_dns_servers();
        std::vector<std::string> Debug_Get_search_domains();
        virtual std::string Dump();
#endif


    private:
        std::mutex change_guard;
        std::vector<std::string> unprocessed_lines;
        std::vector<std::string> vpn_name_servers;
        std::vector<std::string> vpn_name_servers_removed;
        std::vector<std::string> vpn_search_domains;
        std::vector<std::string> vpn_search_domains_removed;
        std::vector<std::string> sys_name_servers;
        std::vector<std::string> sys_search_domains;
        std::vector<NetCfgChangeEvent> notification_queue;
        bool dns_scope_non_global = false;
        unsigned int modified_count = 0;


        /**
         *  Extremely simplistic resolv.conf parser.  It extracts the
         *  information it needs and preserves everything else (except
         *  comment lines) if the file gets regenerated.  This parser
         *  does not preserve any specific order between various options
         *  in the file.
         */
        void parse();


        /**
         *  Generates the contents needed for a file write (@Write())
         *  The data to be written to disk will be in the resolv.conf format
         */
        void generate();
    };
} // namespace DNS
} // namespace NetCfg
