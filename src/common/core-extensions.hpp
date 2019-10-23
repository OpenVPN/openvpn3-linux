//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
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

#ifndef OPENVPN3_CORE_EXTENSIONS
#define OPENVPN3_CORE_EXTENSIONS

#include <iostream>
#include <json/json.h>
#include <openvpn/client/cliconstants.hpp>
#include <openvpn/common/options.hpp>
#include <openvpn/options/merge.hpp>

namespace openvpn {
    inline bool optparser_inline_file(std::string optname)
    {
        return ((optname == "ca")
                || (optname == "key")
                || (optname == "extra-certs")
                || (optname == "cert")
                || (optname == "dh")
                || (optname == "pkcs12")
                || (optname == "tls-auth")
                || (optname == "tls-crypt")) ? true : false;
    }

    std::string optparser_mkline(std::string optname, std::string optvalue)
    {
        bool inlined_file = optparser_inline_file(optname);
        std::stringstream ret;

        if (inlined_file)
        {

            ret << "<" << optname << ">" << std::endl
                << optvalue
                << "</" << optname << ">"
                << std::endl;
        }
        else
        {
            ret << optname << " " << optvalue << std::endl;
        }
        return ret.str();
    }

    class OptionListJSON : public openvpn::OptionList
    {
    public:
        Json::Value json_export() const
        {
            Json::Value outdata;

            for(const auto& element : *this) {
                // Render the complete option line and split out the
                // first "token" as the option name and the rest as the
                // option value
                std::istringstream line(element.render(Option::RENDER_PASS_FMT));
                std::string optname;
                line >> optname;

                // Skip the leading whitespace
                line.seekg(line.tellg()+std::streampos(1));
                std::ostringstream value;
                value << line.rdbuf();

                // Put these parts into the Json::Value storage
                // and trim trailing spaces in value
                outdata[optname] = value.str().substr(0,value.str().find_last_not_of(" ")+1);
            }

            return outdata;
        }

        std::string string_export()
        {
            std::stringstream cfgstr;

            for(const auto& element : *this)
            {
                std::string optname = element.ref(0);
                if (optparser_inline_file(optname))
                {
                    cfgstr << optparser_mkline(optname, element.ref(1)) << std::endl;
                }
                else
                {
                    cfgstr << element.escape() << std::endl;
                }
             }

            return cfgstr.str();
        }
    };

    class ProfileMergeJSON : public openvpn::ProfileMerge
    {
    public:
        ProfileMergeJSON(const Json::Value& data)
        {
            // Parse the JSON input into a plain/text config file which
            // is then parsed/imported into the OpenVPN option storage.
            std::stringstream config_str;
            for(auto it = data.begin(); it != data.end(); ++it)
            {
                std::string name = it.name();
                config_str << optparser_mkline(name, data[name].asString());
            }
            expand_profile(config_str.str(), "", openvpn::ProfileMerge::FOLLOW_NONE,
                           openvpn::ProfileParseLimits::MAX_LINE_SIZE,
                           openvpn::ProfileParseLimits::MAX_PROFILE_SIZE,
                           profile_content().size());
        }
    };
}

#endif // OPENVPN3_CORE_EXTENSIONS

