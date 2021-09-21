//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2020         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020         David Sommerseth <davids@openvpn.net>
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
 * @file   configfileparser.hpp
 *
 * @brief  Simple JSON based configuration file parser, targeting to
 *         integrate easily with the SingleCommand command line parser
 */


#pragma once

#include "config.h"
#include <memory>
#include <string>
#include <vector>
#include <json/json.h>

#include "common/cmdargparser-exceptions.hpp"

namespace Configuration {

/**
 *  Definition of value types used by the configuration file and how
 *  it will be interpreted by the command-line argument parser.
 */
enum class OptionValueType : uint8_t {
    Int,      ///< Integer
    String,   ///< String
    Present   ///< Option is present, with not value argument
};


/**
 *
 *   A single mapping between a command line option and the field it
 *   is stored in in the JSON based configuration file
 */
struct OptionMapEntry
{
    OptionMapEntry(std::string option, std::string file_label,
                   std::string description, OptionValueType type);

    OptionMapEntry(std::string option, std::string file_label,
                   std::string exclusive_group,
                   std::string description, OptionValueType type);


    friend std::ostream& operator<<(std::ostream& os, const OptionMapEntry& e)
    {
        std::stringstream out;
        if (e.present)
        {
            out << e.description << ": ";
            switch (e.type)
            {
            case OptionValueType::Int:
            case OptionValueType::String:
                out << e.value;
                break;

            case OptionValueType::Present:
                if ( e.present )
                {
                    out << (e.present_value ? "Yes" : "No");
                }
                else
                {
                    out << "(not set)";
                }
                break;
            }
            out << std::endl;
        }
        return os << out.str();
    }


    std::string option;            ///< Command line option name
    std::string field_label;       ///< Configuration file entry label
    std::string description;       ///< User friendly description
    std::string exclusive_group;   ///< Belongs to a group with only one can be used
    OptionValueType type;          ///< Data type of this value
    bool present;                  ///< Has this option been configured?
    bool present_value;            ///< Should the option be considered set or unset?
    std::string value;             ///< Value of the setting
};
typedef std::vector<OptionMapEntry> OptionMap;


/**
 *  Generic JSON based configuration file parser which maps command line
 *  arguments with a field in the JSON configuration file.
 *
 *  This is a pure virtual class and the ConfigureMapping() method needs to
 *  to be implemented.
 */
class File
{
public:
    typedef std::shared_ptr<File> Ptr;

    File();
    virtual ~File() = default;


    /**
     *  Parses configuration data from JSON::Value directly
     *
     * @param config  JSON::Value containing configuration data
     */
    virtual void Parse(Json::Value& config);

    /**
     *  Loads a JSON configration file and parses it
     *
     * @param cfgfile  std::string containing the filename to parse
     * @throws ConfigFileException if there were issues opening or parsing the
     *         configuration file
     */
    void Load(const std::string& cfgfile);


    /**
     *  Retrieve an array of configured options
     *
     * @param  all_configured (optional) Boolean flag, defaults to false.
     *                        If set to true, all configured options,
     *                        regardless if they are present in the config
     *                        file or not, will be listed.
     *
     * @return Returns std::vector<std::string> of all option key names
     *         available in the configured OptionMap
     */
    std::vector<std::string> GetOptions(bool all_configured = false);

    /**
     *  Check if an option key is present or not
     *
     * @param key    std::string containing the key to the value to check
     * @return  Returns true if a value to the key is present.
     * @throws  Throw OptionNotFound if the key cannot be found.
     */
    bool IsPresent(const std::string& key);

    /**
     *  Retrieve the value of a specific configuration key
     *
     * @param key  std::string containing the key to the value to look up
     * @return  Returns std::string containing the value if the value is
     *          present.  For OptionValueType::Present and empty string is
     *          returned only if it is present.
     * @throws  Throws OptionNotFound if key is not found or OptionNotPresent
     *          if the the value type is OptionValueType::Present and the
     *          value is not present.
     */
    std::string GetValue(const std::string& key);

    /**
     *  Sets a value to a configuration option in the configuration file
     *
     * @param key   std::string of the command line option name to set
     * @param value std::string of the value to use.  For options of the
     *              OptionValueType::Present, the value must be "1" or "yes"
     *              to be considered set, otherwise it is considered unset.
     *              If the value string is empty, the value is considered
     *              unset/reset.
     */
    void SetValue(const std::string& key, const std::string& value);


    /**
     *  Unsets a configuration option in the configuration file
     *
     * @param key   std::string of the command line option name to unset
     */
    void UnsetOption(const std::string& key);

    /**
     *  Run a check in all options to see if options belonging to the same
     *  group of exclusive options are only used once.
     *
     *  @throws ExclusiveOptionError if two or more options within the
     *          same exclusive_group is found.
     */
    void CheckExclusiveOptions();

    /**
     *  Returns a list of other options in the same exclusive option group
     *  as the given option
     *
     * @param option  std::string of the option to check
     * @return Returns a std::vector<std::string> containing other options in
     *         tagged with the same exclusive_group
     */
    std::vector<std::string> GetRelatedExclusiveOptions(const std::string option);

    /**
     *  Generates a Json::Value based configuration file based on the values
     *  already set.
     *
     * @return  JSON::Values containing a prepared configuration file.
     */
    Json::Value Generate();

    /**
     *  Writes the configuration file containing the currently set values
     *  to a file.
     *
     * @param cfgfname  std::string of the filename to use when saving
     *                  the file.
     */
    void Save(const std::string cfgfname);

    /**
     *  Check if the configuration contains anything.  If all options are
     *  empty/not-set, it is considered empty.
     *
     * @return  Returns true if no entries in the configuration is set.
     */
    bool empty() const;

    /**
     *  Stream helper - formats all the set configuration options and
     *  their values in a user friendly format.
     */
    friend std::ostream& operator<<(std::ostream& os, const File& m)
    {
        std::stringstream out;
        for (auto& e : m.map)
        {
            out << e;
        }
        return os << out.str();
    }

#ifdef OPENVPN_DEBUG
    void Dump() const;
#endif


protected:
    /**
     *  This method need to return the configuration between the
     *  command-line option names and their respective configuration file
     *  label as well as value types and a human-readable description of the
     *  value.
     *
     *  This method must be implemented by the user of this File object
     *
     * @return Must return a OptionMap to use.
     */
    virtual OptionMap ConfigureMapping() = 0;


private:
    bool map_configured = false;   ///< Has ConfigureMapping() been run?
    OptionMap map;           ///< Currently active configuration map

    /**
     *  Internal helper method, ensuring the mapping has been set up
     */
    void configure_mapping();

}; // class File
}  // namespace Configuration
