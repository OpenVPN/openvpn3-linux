//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2020 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   core-extensions.cpp
 *
 * @brief  Unit test for common/core-extensions.hpp
 */

#include <string>
#include <sstream>

#include "openvpn/log/logsimple.hpp"

#include <gtest/gtest.h>
#include "common/core-extensions.hpp"


namespace unittest {

using namespace openvpn;

class AccessServerMetaOpts : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        std::stringstream metaopts;
        metaopts << "# OVPN_ACCESS_SERVER_ALLOW_UNSIGNED=-22" << std::endl
                 << "# OVPN_ACCESS_SERVER_APP_VERIFY_START" << std::endl
                 << "# Testing test test 1" << std::endl
                 << "# Testing test test 2" << std::endl
                 << "# Testing test test 3" << std::endl
                 << "# OVPN_ACCESS_SERVER_APP_VERIFY_STOP" << std::endl
                 << "# OVPN_ACCESS_SERVER_AUTOLOGIN_SPEC=123" << std::endl
                 << "# OVPN_ACCESS_SERVER_CLI_PREF_ALLOW_WEB_IMPORT=1" << std::endl
                 << "# OVPN_ACCESS_SERVER_CLI_PREF_BASIC_CLIENT=1" << std::endl
                 << "# OVPN_ACCESS_SERVER_CLI_PREF_ENABLE_CONNECT=1" << std::endl
                 << "# OVPN_ACCESS_SERVER_CLI_PREF_ENABLE_XD_PROXY=1" << std::endl
                 << "# OVPN_ACCESS_SERVER_DYNAMIC=123" << std::endl
                 << "# OVPN_ACCESS_SERVER_EXTERNAL_PKI=1" << std::endl
                 << "# OVPN_ACCESS_SERVER_GENERIC=99" << std::endl
                 << "# OVPN_ACCESS_SERVER_HOST_FIELD=1" << std::endl
                 << "# OVPN_ACCESS_SERVER_HOST_LIST_START" << std::endl
                 << "# host-entry-1" << std::endl
                 << "# host-entry-2" << std::endl
                 << "# host-entry-3" << std::endl
                 << "# OVPN_ACCESS_SERVER_HOST_LIST_STOP" << std::endl
                 << "# OVPN_ACCESS_SERVER_SITE_LIST_START" << std::endl
                 << "# site-entry-1" << std::endl
                 << "# site-entry-2" << std::endl
                 << "# site-entry-3" << std::endl
                 << "# OVPN_ACCESS_SERVER_SITE_LIST_STOP" << std::endl
                 << "# OVPN_ACCESS_SERVER_ICON_CONTENT_TYPE=text/plain" << std::endl
                 << "# OVPN_ACCESS_SERVER_ICON_DATA_START" << std::endl
                 << "# some strange icon data" << std::endl
                 << "# OVPN_ACCESS_SERVER_ICON_DATA_STOP" << std::endl
                 << "# OVPN_ACCESS_SERVER_IS_OPENVPN_WEB_CA=42" << std::endl
                 << "# OVPN_ACCESS_SERVER_NO_WEB=98" << std::endl
                 << "# OVPN_ACCESS_SERVER_ORGANIZATION=Core Extension Unit Test" << std::endl
                 << "# OVPN_ACCESS_SERVER_PORTAL_URL=http://portal.example.org/" << std::endl
                 << "# OVPN_ACCESS_SERVER_WEB_CA_BUNDLE_START" << std::endl
                 << "#  some web ca certificate bundle comes here" << std::endl
                 << "# OVPN_ACCESS_SERVER_WEB_CA_BUNDLE_STOP" << std::endl
                 << "# OVPN_ACCESS_SERVER_WSHOST=wshost.example.org" << std::endl
                 << "# OVPN_ACCESS_SERVER_WSHOST_ANY=3" << std::endl
                 << "# OVPN_ACCESS_SERVER_AUTOLOGIN=1" << std::endl
                 << "# OVPN_ACCESS_SERVER_FRIENDLY_NAME=Core Extension unit-test" << std::endl
                 << "# OVPN_ACCESS_SERVER_PROFILE=profile-value" << std::endl
                 << "# OVPN_ACCESS_SERVER_USERNAME=username-value" << std::endl;

        OptionList::Limits limits("profile is too large",
                                  ProfileParseLimits::MAX_PROFILE_SIZE,
                                  ProfileParseLimits::OPT_OVERHEAD,
                                  ProfileParseLimits::TERM_OVERHEAD,
                                  ProfileParseLimits::MAX_LINE_SIZE,
                                  ProfileParseLimits::MAX_DIRECTIVE_SIZE);

        options.parse_from_config(metaopts.str(), &limits);
        options.parse_meta_from_config(metaopts.str(), "OVPN_ACCESS_SERVER", &limits);
        options.update_map();
    }

  public:
    struct _option_tests
    {
        std::string key = {};
        std::string value = {};
    };
    using option_tests = std::vector<struct _option_tests>;

    OptionListJSON options;


    const option_tests GetMustFindOptions() const
    {
        // These meta-options and values is expected to be
        // available after parsing the configuration
        return {
            {"AUTOLOGIN", "1"},
            {"FRIENDLY_NAME", "Core Extension unit-test"},
            {"PROFILE", "profile-value"},
            {"USERNAME", "username-value"}};
    }

    const std::vector<std::string>
    GetIgnoredOptions() const
    {
        // All of these meta-options should be filtered out and
        // should not be found in the string_export() and json_export()
        // functions.  They should still be available by accessing them
        // directly via the OptionList object.
        return {
            "ALLOW_UNSIGNED",
            "APP_VERIFY",
            "AUTOLOGIN_SPEC",
            "CLI_PREF_ALLOW_WEB_IMPORT",
            "CLI_PREF_BASIC_CLIENT",
            "CLI_PREF_ENABLE_CONNECT",
            "CLI_PREF_ENABLE_XD_PROXY",
            "DYNAMIC",
            "EXTERNAL_PKI",
            "GENERIC",
            "HOST_FIELD",
            "HOST_LIST",
            "SITE_LIST",
            "ICON_CONTENT_TYPE",
            "ICON_DATA",
            "IS_OPENVPN_WEB_CA",
            "NO_WEB",
            "ORGANIZATION",
            "PORTAL_URL",
            "WEB_CA_BUNDLE",
            "WSHOST",
            "WSHOST_ANY"};
    }

  private:
};

TEST_F(AccessServerMetaOpts, parse_meta)
{
    for (const auto &tc : GetMustFindOptions())
    {
        Option opt = options.get(tc.key);

        EXPECT_STREQ(tc.key.c_str(), opt.get(0, 64).c_str());
        EXPECT_STREQ(tc.value.c_str(), opt.get(1, 64).c_str());
    }

    // All meta variabibles will be visible in the openvpn::OptionList
    // class (which OptionListJSON extends).  Check that all of them
    // are present
    for (const auto &key : GetIgnoredOptions())
    {
        const Option *opt = options.get_ptr(key);
        EXPECT_NE(opt, (Option *)NULL)
            << "Option '" << key << "'"
            << " was not found in the parsed OptionListJSON object";
    }
}

TEST_F(AccessServerMetaOpts, json_export)
{
    Json::Value json = options.json_export();
    Json::Value::Members jm = json.getMemberNames();
    std::vector<std::string> valid_keys;

    // Check for meta-options which must be present
    for (const auto &tc : GetMustFindOptions())
    {
        const auto f = std::find(jm.begin(), jm.end(), tc.key);
        EXPECT_NE(f, jm.end())
            << "Could not find '" << tc.key << "' in json_export() data";

        const char *exp_value = tc.value.c_str();
        EXPECT_STREQ(exp_value, json[tc.key][0].asCString());
        valid_keys.push_back(tc.key);
    }

    // Check that ignored options cannot be found
    for (const auto &ign : GetIgnoredOptions())
    {
        const auto f = std::find(jm.begin(), jm.end(), ign);
        EXPECT_EQ(f, jm.end())
            << "Meta-option '" << ign
            << "' was found unexpectedly in json_export() data";
        valid_keys.push_back(ign);
    }

    // Check that the Json export does not contain anything extra
    for (const auto &k : jm)
    {
        const auto f = std::find(valid_keys.begin(), valid_keys.end(), k);
        EXPECT_NE(f, valid_keys.end())
            << "Option '" << k << "' was found unexpectedly in the json_export() data";
    }
}

TEST_F(AccessServerMetaOpts, string_export)
{
    // Export the parsed config as a string and process each line
    std::istringstream cfg(options.string_export());
    std::string line;

    const auto must_have = GetMustFindOptions();
    const auto ignored = GetIgnoredOptions();
    while (std::getline(cfg, line))
    {
        // Extract the option keyword
        std::stringstream l(line);
        std::string o;

        l >> o; // this should be 'setenv'
        EXPECT_STREQ("setenv", o.c_str());

        l >> o; // this should be 'opt'
        EXPECT_STREQ("opt", o.c_str());

        // Look up the option keyword in the list of must have options.
        // It must be found here.
        l >> o; // this should contain the meta-option name
        const auto mh = std::find_if(must_have.begin(),
                                     must_have.end(),
                                     [o](const struct _option_tests &tc)
                                     {
            return tc.key == o;
        });
        EXPECT_NE(mh, must_have.end())
            << "Option '" << o << "' was found unexpectedly in string_export() data";

        // Look up the option keyworkd in the list of ignored options;
        // It should not appear here at all
        const auto ign = std::find(ignored.begin(), ignored.end(), o);
        EXPECT_EQ(ign, ignored.end())
            << "Option '" << o << "' should not appear in string_export() data";
    }
}

} // namespace unittest
