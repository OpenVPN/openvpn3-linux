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
        std::string json_export()
        {
            Json::Value outdata;

            // FIXME: Looks very hacky
            for(auto element : *this) {
                outdata[element.ref(0)] = (element.size() > 1 ? element.ref(1) : "");
            }

            std::stringstream output;
            output << outdata;
            return output.str();
        }

        std::string string_export()
        {
            std::stringstream cfgstr;

            // FIXME: Looks very hacky
            for(auto element : *this)
            {
                std::string optname = element.ref(0);
                std::string params;
                bool opened = false;
                for (int i = 1; i < element.size(); i++)
                {
                    // FIXME: This *is* hacky.  But needed until
                    // FIXME: ParseClientConfig have been revamped
                    if (optname == "static-challenge")
                    {
                        if ((1 == i && !opened)
                            || (i == element.size()-1 && opened))
                        {
                            params += "\"";
                            if (opened)
                            {
                                params += " ";
                            }
                            opened = !opened;

                        }
                    }
                    params += element.ref(i);
                    if (!optparser_inline_file(optname))
                    {
                        params += " ";
                    }
                }
                cfgstr << optparser_mkline(optname, params);
             }

            return cfgstr.str();
        }
    };

    class ProfileMergeJSON : public openvpn::ProfileMerge
    {
    public:
        ProfileMergeJSON(const std::string json_str)
        {
            // Read the JSON formatted input string
            // and parse it into a JSON::Value object.
            // This has to go through a std::stringstream, as
            // the JSON parser is stream based.
            std::stringstream json_stream;
            json_stream << json_str;  // Convert std::string to std::stringstream
            Json::Value data;
            json_stream >> data;      // Parse into JSON::Value


            // Parse the JSON input into a plain/text config file which
            // is then parsed/imported into the OpenVPN option storage.
            std::stringstream config_str;
            for( Json::ValueIterator it = data.begin();
                 it != data.end(); ++it) {
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

