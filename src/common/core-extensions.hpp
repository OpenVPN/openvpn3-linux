//  OpenVPN 3 Linux client -- Next generation OpenVPN clientq
//
//  Copyright (C) 2017 - 2021  OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017 - 2021  David Sommerseth <davids@openvpn.net>
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

#pragma once
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
                || (optname == "auth-user-pass")
                || (optname == "http-proxy-user-pass")
                || (optname == "dh")
                || (optname == "pkcs12")
                || (optname == "tls-auth")
                || (optname == "tls-crypt")
                || (optname == "tls-crypt-v2")) ? true : false;
    }

    inline bool option_req_array(const std::string& optname)
    {
        return (("peer-fingerprint" == optname)
                || ("pull-filter" == optname)
                || ("remote" == optname)
                || ("route" == optname)
                || ("route-ipv6" == optname)
               ) ? true : false;
    }

    std::string optparser_mkline(std::string optname, std::string optvalue)
    {
        bool inlined_file = optparser_inline_file(optname);
        std::stringstream ret;

        if (inlined_file && !optvalue.empty())
        {
            ret << "<" << optname << ">" << std::endl
                << optvalue;
            if ('\n' != optvalue.back())
            {
                ret << std::endl;
            }
            ret << "</" << optname << ">"
                << std::endl;
        }
        else
        {
            ret << optname << " " << optvalue << std::endl;
        }
        return ret.str();
    }

    /**
     *  This class extens the OptionList class with JSON import/export
     *  capabilities as well as a utility function to export the whole
     *  configuration file as a string.
     *
     *  There are tree formatting types of the JSON formatting, due to
     *  the evolving of OpenVPN 3 Linux.  The export will only use the
     *  format 3a/3b type, but the import should be capable of handling
     *  all versions.
     *
     *  1.  key:value
     *  2.  key:[value, value, ...] - used for option_req_array() opts
     *  3a. key:[value, ...]
     *  3b. key:[[value, value, ...]] - used for option_req_array() opts
     *
     *  1) is the original format and 2) is an extension of it to
     *  properly support configuration profiles with some options
     *  listed multiple times.  Each value field contains everything after
     *  the option name.
     *
     *  Examples:
     *
     *      key-direction 1
     *           >>>> key="key-direction", value="1"
     *
     *      static-challenge "Enter OTP code" 1
     *           >>>> key="static-challenge",
     *                value="\"Enter OTP code\" 1"
     *
     *      remote server1.example.net 1194 udp
     *      remote server2.example.net 1194 tcp
     *           >>>> key="remote",
     *                value=["server1.example.net 1194 udp",
     *                       "server2.example.net 1194 tcp"]
     *
     *  Notice the string quoting required in the example above.
     *  The --remote example above uses format 2).  Before this
     *  format only the last parsed --remote option was preserved
     *  in the JSON export.
     *
     *  Format 3a) and 3b) uses an array for all the values.  This
     *  is to be able to have a more direct mapping to Option::data.
     *  This also removes the need for the string quoting.
     *
     *  Examples:
     *
     *      key-direction 1
     *           >>>> key="key-direction", value=["1"]
     *
     *      static-challenge "Enter OTP code" 1
     *           >>>> key="static-challenge",
     *                value=["Enter OTP code", "1"]
     *
     *      remote server1.example.net 1194 udp
     *      remote server2.example.net 1194 tcp
     *           >>>> key="remote",
     *                value=[["server1.example.net", "1194", "udp"],
     *                       ["server2.example.net", "1194", "tcp"]]
     *
     *
     */
    class OptionListJSON : public openvpn::OptionList
    {
    public:
        Json::Value json_export() const
        {
            Json::Value outdata;

            // Iterate all the std::vector<Option> objects
            // OptionListJSON inherits OptionList which again inherits
            // std::vector<Option>, which is why we access the std::vector
            // via *this.
            for(const auto& element : *this) {
                std::string optname(element.ref(0));

                Json::Value optval{};
                if (optparser_inline_file(optname))
                {
                    // Inlined files needs to be rendered via the
                    // Option::render() method.  We remove the option name
                    // from Option object first, to avoid getting that into the
                    // option value we will store in the JSON dictionary.
                    Option o(element);
                    o.remove_first(1);
                    optval.append(o.render(Option::RENDER_PASS_FMT));
                }
                else
                {
                    // For all other options, we extract each Option value
                    // element, skipping the first element (option name) to
                    // avoid getting that duplicated into the option value
                    // stored in the JSON dictionary
                    for (size_t i = 1; i < element.size(); i++)
                    {
                        optval.append(element.ref(i));
                    }
                }

                if (!option_req_array(optname))
                {
                    // For options only expected to be used once, they get
                    // the value array directly.  This is format 3a) as
                    // described in OptionListJSON::json_import()
                    outdata[optname] = optval;
                }
                else
                {
                    // Certain options can be used multiple times, so we wrap
                    // these options into to another array layer.  This is
                    // format 3b) as described in OptionListJSON::json_import()
                    outdata[optname].append(optval);
                }
            }

            // The JSON blob is ready.
            return outdata;
        }


        void json_import(const Json::Value& data)
        {
            for(auto it = data.begin(); it != data.end(); ++it)
            {
                std::string optname = it.name();
                std::string optval{};


                // This is a bit convoluted, but this is to ensure
                // we can handle all the 3 types of configuration JSON formats
                // at import time.

                if (data[optname].isArray())
                {
                    // This is either an option present several times
                    // or it's the new JSON formatting where each option
                    // value is stored as an array element.  If the content
                    // if the first element is also an array, it's multiple
                    // options.
                    //
                    //  This covers format type 2), 3a) or 3b)
                    //

                    if (data[optname][0].isArray())
                    {
                        // Options with the same name which can be used
                        // more times are stored as an array with an
                        // element per entry for this option.  This is
                        // format 3b)
                        for (const auto& e : data[optname])
                        {
                            // Here the 3b) format is essentially unwrapped
                            // to end up as multple 3a) formatted elements.
                            add_option(optname, e);
                        }
                    }
                    else
                    {
                        if (option_req_array(optname) && data[optname].isArray())
                        {
                            // Old JSON format - with a value array for selected
                            // options.  This is format 2)
                            for (const auto& v : data[optname])
                            {
                                // We need to iterate each element
                                // independently when calling add_option()
                                // otherwise all the elements is preserved in
                                // the same Option object.  We want separate
                                // Option object per element.
                                //
                                // This is similar to the 3b->3a unwrapping
                                // above, just that we do the 2a->1 unwrapping
                                // here.
                                add_option(optname, v);
                            }
                        }
                        else
                        {
                            // This covers the plain type 3a) format
                            add_option(optname, data[optname]);
                        }
                    }
                }
                else
                {
                    // The old original JSON format, where the field value is
                    // single string containing everything.  This is format 1).
                    add_option(optname, data[optname]);
                }
            }
        }


        std::string string_export()
        {
            std::stringstream cfgstr;

            // Iterate all the std::vector<Option> objects
            // OptionListJSON inherits OptionList which again inherits
            // std::vector<Option>, which is why we access the std::vector
            // via *this.
            for(const auto& element : *this)
            {
                std::string optname = element.ref(0);

                // Inlined files needs special treatment, as they span
                // multiple lines.  Just retrieve the raw data directly here.
                if (optparser_inline_file(optname) && element.size() > 1)
                {
                    cfgstr << optparser_mkline(optname, element.ref(1)) << std::endl;
                }
                else
                {
                    // For everything else, we use the Option::escape() method
                    // to render the output we need for the string export of the
                    // profile.
                    cfgstr << element.escape(false) << std::endl;
                }
            }

            return cfgstr.str();
        }

    private:
        /**
         * Parses a Json::Value object and adds it as an Option object to the
         * std::vector<Option> which is part of the OptionList object.
         *
         * This method is capable of parsing format 1) and 3a).  For format
         * 2) and 3b), their values needs to be unwrapped first.
         *
         * @param key   std::string containing the option key name
         * @param data  Json::Value containing the data to associate with this
         *              key name
         */
        void add_option(const std::string& key, const Json::Value& data)
        {
            if (data.isArray())
            {
                // Parse each value element as a separate token for the
                // same option.  This handles format type 3a).
                std::vector<std::string> v;
                for (const auto& e : data)
                {
                    v.push_back(e.asString());
                }
                Option opt(key, v);
                push_back(opt);
                return;
            }

            if (optparser_inline_file(key))
            {
                // Inline files needs a slightly different handling in the
                // type 1) format.  The OptionList::parse_option_from_line()
                // does not expect inline file.
                Option opt(key, data.asString());
                push_back(opt);
                return;
            }

            // This is parsing the type 1) format, where we need to
            // do more string parsing on the complete config line itself to
            // get the Option object we need.
            OptionList::Limits limits("profile is too large",
                                      ProfileParseLimits::MAX_PROFILE_SIZE,
                                      ProfileParseLimits::OPT_OVERHEAD,
                                      ProfileParseLimits::TERM_OVERHEAD,
                                      ProfileParseLimits::MAX_LINE_SIZE,
                                      ProfileParseLimits::MAX_DIRECTIVE_SIZE);

            std::stringstream l;
            l << key << " " << data.asString();
            push_back(OptionList::parse_option_from_line(l.str(), &limits));
        }
    };
}
