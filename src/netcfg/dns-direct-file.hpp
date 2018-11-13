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

/**
 * @file   dns-direct-file.hpp
 *
 * @brief  This will update resolv.conf files directly, in the typical
 *         resolv.conf format.  Normally, this would be /etc/resolv.conf, but
 *         can be used for other files as well, if there is a local
 *         resolver service which picks these files some elsewhere.
 */


#pragma once

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "common/timestamp.hpp"
#include "dns-resolver-settings.hpp"
#include "netcfg-exception.hpp"

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
                      const std::string& backup_filename = "")
            : filename(filename), backup_filename(backup_filename)
        {
        }


        virtual ~FileGenerator()
        {
        }


        FileGenerator(const FileGenerator& origin) noexcept
        {
            file_contents = origin.file_contents;
        }


        virtual void SetFilename(const std::string& fname) noexcept
        {
            filename = fname;
        }

        virtual std::string GetFilename() noexcept
        {
            return filename;
        }


        virtual void SetBackupFilename(const std::string& bfname) noexcept
        {
            backup_filename = bfname;
        }


        virtual std::string GetBackupFilename() noexcept
        {
            return backup_filename;
        }


    protected:
        std::vector<std::string> file_contents;


        /**
         *  Reads the file and calls the @Parse() method on success.
         */
        void Read()
        {
            std::ifstream input(filename);
            for (std::string line; std::getline(input, line);)
            {
                file_contents.push_back(line);
            }
            input.close();
            Parse();
        }


        /**
         *  Writes the file to disk.  If a backup file is requested, ensure
         *  that is arranged before anything else is done.  If a backupfile
         *  already exists with the same filename, the old backup file is
         *  automatically removed.
         *
         *  The @Generate() method will be called before the new file is
         *  being opened and written to disk
         *
         */
        void Write()
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
            Generate();
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
        void RestoreBackup()
        {
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
         *  This method is responsible to parse the contents of the
         *  file_contents variable.  This method must be implemented.
         */
        virtual void Parse() = 0;


        /**
         *  This method is responsible to generate the contents of the
         *  file to be written to disk.  The file contents needs to be
         *  stored in the file_contents variable.
         */
        virtual void Generate() = 0;


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
        bool file_exists (const std::string& fname) noexcept
        {
            struct stat buffer;
            return (stat (fname.c_str(), &buffer) == 0);
        }
    };




    /**
     *  Class able to read, parse, manipulate and write resolv.conf files.
     *  This builds on the @FileGenerator and @ResolverSettings classes
     */
    class ResolvConfFile : public FileGenerator,
                           public ResolverSettings
    {
    public:
        /**
         *  Instantiate a resolv.conf parser
         *
         * @param filename  std::string of full path to the resolv.conf
         *                  file to parse
         * @param backup    bool, if a backup should be made automatically
         *                  on overwrites (default: false)
         */
        ResolvConfFile(const std::string& filename,
                       const std::string& backup_filename = "")
            : FileGenerator(filename, backup_filename)
        {
        }


        virtual ~ResolvConfFile()
        {
        }


        virtual std::string GetBackendInfo()
        {
            std::string ret = std::string("ResolvConf file backend. Using: ")
                            + "'" + GetFilename() + "'";
            if (!GetBackupFilename().empty())
            {
                ret += "  Backup file: '" + GetBackupFilename() + "'";
            }
            return ret;
        }


        virtual void Fetch()
        {
            Read();
            modified = false;
        }


        virtual void Apply()
        {
            CommitChanges();
            Write();
            modified = false;
        }


        virtual void Restore()
        {
            RestoreBackup();
            // The old config is restored, most likely is different
            // from the config in this object
            modified = true;
        }


#ifdef ENABLE_DEBUG
        /**
         *  Simple debug method to just dump the contents of the resolf.conf
         *  file contents being held in memory.
         *
         *  @returns Returns a formatted string of the parsed contents
         */
        virtual std::string Dump()
        {
            std::stringstream ret;

            int i = 0;
            for (const auto& e : dns_search)
            {
                ret << "dns_search  [" << ++i << "]: " << e << std::endl;
            }

            i = 0;
            for (const auto& e : dns_servers)
            {
                ret << "dns_servers [" << ++i << "]: " << e << std::endl;
            }
            return ret.str();
        }
#endif


    protected:
        /**
         *  Extremely simplistic resolv.conf parser.  It extracts the
         *  information it needs and preserves everything else (except
         *  comment lines) if the file gets regenerated.  This parser
         *  does not preserve any specific order between various options
         *  in the file.  Only arguments to specific options preserves the
         *  order.
         */
        void Parse()
        {
            ClearDNSServers();
            ClearDNSSearch();

            for (const auto& line : file_contents)
            {
                if (line.find("nameserver ") == 0)
                {
                    dns_servers.push_back(line.substr(11));
                }
                else if (line.find("search ") == 0)
                {
                    size_t idx = 0;
                    do
                    {
                        idx = line.find(" ", idx+1);
                        if (std::string::npos != idx)
                        {
                            dns_search.push_back(line.substr(idx+1, line.substr(idx+1).find(" ")));
                        }
                    } while( std::string::npos != idx );
                }
                else if (line.find("#") == 0)
                {
                    // We ignore comment lines
                }
                else
                {
                    // Put aside all other lines, which will be written back
                    // to the file when Write() called
                    unprocessed_lines.push_back(line);
                }
            }
        }

        /**
         *  Generates the contents needed for a file write (@Write())
         *  which uses the resolv.conf format.  It
         */
        void Generate()
        {
            file_contents.clear();

            file_contents.push_back("#");
            file_contents.push_back("# Generated by NetCfg::DNS::ResolvConfFormat");
            file_contents.push_back("# " + GetTimestamp());
            file_contents.push_back("#");

            std::stringstream dns_search_line;
            for (const auto& e : dns_search)
            {
                dns_search_line << " " << e;
            }
            if (!dns_search_line.str().empty())
            {
                file_contents.push_back("search" + dns_search_line.str());
            }

            for (const auto& e : dns_servers)
            {
                file_contents.push_back("nameserver " + e);
            }

            for (const auto& e : unprocessed_lines)
            {
                file_contents.push_back(e);
            }
        }


    private:
        std::vector<std::string> unprocessed_lines;
    };

} // namespace DNS
} // namespace NetCfg
