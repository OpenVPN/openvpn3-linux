//  OpenVPN 3 Linux client -- Next generation OpenVPN clientq
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017 - 2023  David Sommerseth <davids@openvpn.net>
//

#pragma once

#include <iostream>
#include <unordered_set>
#include <json/json.h>

#include <openvpn/client/cliconstants.hpp>
#include <openvpn/common/options.hpp>
#include <openvpn/options/merge.hpp>

namespace openvpn {
/**
 *  This class extends the OptionList class with JSON import/export
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
        for (const auto &element : *this)
        {
            std::string optname(element.ref(0));

            bool filter_metaopt = std::any_of(
                ignore_as_metaopts.begin(),
                ignore_as_metaopts.end(),
                [&optname](const std::string &elem)
                {
                    return !optname.compare(0, elem.length(), elem);
                });
            if (filter_metaopt)
            {
                continue;
            }

            Json::Value optval{};
            if (is_tag_option(optname))
            {
                // Option values inside tags needs to be rendered via the
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

            if (!option_value_need_array(optname))
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


    void json_import(const Json::Value &data)
    {
        for (auto it = data.begin(); it != data.end(); ++it)
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
                    for (const auto &e : data[optname])
                    {
                        // Here the 3b) format is essentially unwrapped
                        // to end up as multple 3a) formatted elements.
                        add_option(optname, e);
                    }
                }
                else
                {
                    if (option_value_need_array(optname) && data[optname].isArray())
                    {
                        // Old JSON format - with a value array for selected
                        // options.  This is format 2)
                        for (const auto &v : data[optname])
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
        for (const auto &element : *this)
        {
            std::string optname = element.ref(0);

            //
            //  OPENVPN ACCESS SERVER HACK
            //
            //  OpenVPN Access Server profiles can contain meta-options
            //  which are extracted by the OpenVPN 3 Core library from
            //  commented-out lines as key/value pairs, when the key
            //  is prefixed with "OVPN_ACCESS_SERVER_".  The option
            //  parser will use the rest of that key as an option key
            //  and what follows after the first '=' character as value
            //
            //  Due to earlier Access Server profiles would have a lot
            //  of these meta options and most of them are now no longer
            //  used or needed, we filter them out.
            //
            //  This is more or less a hack required when Access Server
            //  is configured to do web authentication, until OpenVPN 3 Core
            //  library handles this use case more similar to how OpenVPN 2.x
            //  behaves, making proper use of the --auth-user-token in this
            //  mode.
            //
            //  Since the list of meta-options to ignore is a list of
            //  option /prefixes/, std::any_of with a lambda is used.
            //
            bool filter_metaopt = std::any_of(
                ignore_as_metaopts.begin(),
                ignore_as_metaopts.end(),
                [&optname](const std::string &elem)
                {
                    return !optname.compare(0, elem.length(), elem);
                });
            if (filter_metaopt)
            {
                continue;
            }

            //  A few of these meta-options from Access Server are used
            //  by the OpenVPN 3 Core library when connecting, so we rewrite
            //  them as "setenv opt KEY VALUE" instead.
            //
            //  This allows the OpenVPN 3 Core library to extract this
            //  information and still not bail out with an error if it
            //  doesn't understand it as a valid option.  This information
            //  may still be used elsewhere in a client implementation.
            //
            static const std::unordered_set<std::string> rewrite_as_metaopts = {
                "AUTOLOGIN",
                "FRIENDLY_NAME",
                "PROFILE",
                "USERNAME"};
            bool rewrite_meta = std::find(rewrite_as_metaopts.begin(),
                                          rewrite_as_metaopts.end(),
                                          optname)
                                != rewrite_as_metaopts.end();

            // Option values inside tags needs special treatment, as they span
            // multiple lines.  Just retrieve the raw data directly here.
            if (is_tag_option(optname) && element.size() > 1)
            {
                cfgstr << compose_cfg_line(optname, element.ref(1)) << std::endl;
            }
            else if (rewrite_meta)
            {
                cfgstr << "setenv opt " << element.escape(false) << std::endl;
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
    // FIXME: Hackish workaround for OpenVPN Access Server
    //        The list of options here are more to be considered
    //        wildcard matches
    //
    const std::unordered_set<std::string> ignore_as_metaopts = {
        "ALLOW_UNSIGNED",
        "APP_VERIFY",
        "AUTOLOGIN_SPEC",
        "CLI_PREF_",
        "DYNAMIC",
        "EXTERNAL_PKI",
        "GENERIC",
        "HOST_FIELD",
        "HOST_LIST",
        "SITE_LIST",
        "ICON_",
        "IS_",
        "NO_WEB",
        "ORGANIZATION",
        "PORTAL_URL",
        "WEB_CA",
        "WSHOST"};


    /**
     *  Check if an option is to be preserved as a "tag block"
     *  instead of just a single option line.
     *
     *  This is used when recreating the configuration profile
     *  as a plain-text file.
     *
     * @param optname std::string with option to look up
     * @return bool, true if the option is to be handled as a "tag option".
     */
    bool is_tag_option(const std::string &optname) const
    {
        static const std::unordered_set<std::string> tag_opts{
            "ca",
            "cert",
            "connection",
            "dh",
            "extra-certs",
            "http-proxy-user-pass",
            "key",
            "pkcs12",
            "tls-auth",
            "tls-crypt",
            "tls-crypt-v2"};

        return tag_opts.find(optname) != tag_opts.end();
    }


    /**
     *  Check if an option may require its values to be stored
     *  in an array/vector.
     *
     *  This is required for options which can appear more times
     *  in the same configuration file, since key/value storages
     *  (like dictionaries) cannot have duplicated keys.
     *
     * @param optname std::string with option key to look up
     * @return bool, true if value storage should be in an array
     */
    bool option_value_need_array(const std::string &optname) const
    {
        static const std::unordered_set<std::string> array_storage{
            "connection",
            "peer-fingerprint",
            "pull-filter",
            "remote",
            "route",
            "route-ipv6"};

        return array_storage.find(optname) != array_storage.end();
    }


    /**
     *  Compose a configuration string for a single option key/value
     *  which follows the standard OpenVPN configuration file format
     *
     * @param optname    std::string of the option key
     * @param optvalue   std::string with the option value to use
     * @return std::string containing the properly formatted
     *         configuration line
     */
    std::string compose_cfg_line(const std::string &optname, const std::string &optvalue) const
    {
        std::stringstream ret;

        if (is_tag_option(optname) && !optvalue.empty())
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
    void add_option(const std::string &key, const Json::Value &data)
    {
        if (data.isArray())
        {
            // Parse each value element as a separate token for the
            // same option.  This handles format type 3a).
            std::vector<std::string> v;
            for (const auto &e : data)
            {
                v.push_back(e.asString());
            }
            Option opt(key, v);
            push_back(opt);
            return;
        }

        if (is_tag_option(key))
        {
            // Option values inside tags needs a slightly different handling
            // in the type 1) format.  The OptionList::parse_option_from_line()
            // does not expect tag-formatted key/values.
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
} // namespace openvpn
