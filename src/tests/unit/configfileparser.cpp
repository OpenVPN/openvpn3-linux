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
 * @file   configfileparser.cpp
 *
 * @brief  Unit test for Configuration::File and related classes
 */

#include <algorithm>
#include <memory>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <json/json.h>

#include <gtest/gtest.h>
#include "common/configfileparser.hpp"

using namespace Configuration;

namespace unittest {

//
//   OptionMapEntry unit tests
//

TEST(OptionMapEntry, stringstream_string)
{
    OptionMapEntry t1_str{"cmdline", "command_line",
                          "Simple Description", OptionValueType::String};
    std::stringstream t1_test;
    t1_test << t1_str;
    std::string t1_expect{""};
    ASSERT_EQ(t1_test.str(), t1_expect) << "Unset value failed";

    t1_str.value = "Test Value";
    ASSERT_EQ(t1_test.str(), t1_expect) << "Value set, incorrect present value failed";

    t1_str.present = true;
    std::string t1_test2{"Simple Description: Test Value\n"};
    ASSERT_EQ(t1_test.str(), t1_expect) << "Incorrect value rendering";
}


TEST(OptionMapEntry, stringstream_integer)
{
    //  This test should normally not fail if stringstream_string passes,
    //  as values are stored as strings.

    OptionMapEntry t1_str{"cmdline", "command_line",
                          "Simple Description", OptionValueType::Int};
    std::stringstream t1_test;
    t1_test << t1_str;
    std::string t1_expect{""};
    ASSERT_EQ(t1_test.str(), t1_expect) << "Unset value failed";

    t1_str.value = "123";
    ASSERT_EQ(t1_test.str(), t1_expect) << "Value set, incorrect present value failed";

    t1_str.present = true;
    std::string t1_test2{"Simple Description: 123\n"};
    ASSERT_EQ(t1_test.str(), t1_expect) << "Incorrect value rendering";
}


TEST(OptionMapEntry, stringstream_present)
{
    //  This test does not care about the value field at all, only the
    //  present flag - which returns a "Yes" in the rendering if true.

    OptionMapEntry t1_str{"cmdline", "command_line",
                          "Simple Description", OptionValueType::Present};
    std::stringstream t1_test;
    t1_test << t1_str;
    std::string t1_expect{""};
    ASSERT_EQ(t1_test.str(), t1_expect) << "Unset value failed";

    t1_str.value = "Test Value";
    ASSERT_EQ(t1_test.str(), t1_expect)
        << "Value set with incorrect present value failed";

    t1_str.present = true;
    std::string t1_test2{"Simple Description: Yes\n"};
    ASSERT_EQ(t1_test.str(), t1_expect) << "Incorrect value rendering";
}


//
//   class Configuration::File unit tests
//

class TestFile : public Configuration::File
{
public:
    typedef std::unique_ptr<TestFile> Ptr;

    TestFile() : File()
    {
    }


protected:
    Configuration::OptionMap ConfigureMapping()
    {
        return {
            OptionMapEntry{"integer-option", "int_opt",
                           "Test Option - Integer", OptionValueType::Int},
            OptionMapEntry{"string-option", "str_opt",
                           "Test Option - String", OptionValueType::String},
            OptionMapEntry{"present-option", "present_opt",
                           "Test Option - Present", OptionValueType::Present},
            OptionMapEntry{"not-present-option", "not_present",
                           "Option which should not be present/set",
                           OptionValueType::Present},

            OptionMapEntry{"group-1-opt1", "grp1opt1", "group1",
                           "Test Group 1 Option 1", OptionValueType::Int},
            OptionMapEntry{"group-1-opt2", "grp1opt2", "group1",
                           "Test Group 1 Option 2", OptionValueType::Int},

            OptionMapEntry{"group-2-optA", "grp2optA", "group2",
                           "Test Group 2 Option A", OptionValueType::String},
            OptionMapEntry{"group-2-optB", "grp2optB", "group2",
                           "Test Group 2 Option B", OptionValueType::String},
            OptionMapEntry{"group-2-optC", "grp2optC", "group2",
                           "Test Group 2 Option C", OptionValueType::Present}
        };
    }
};


class ConfigurationFile : public ::testing::Test
{
protected:
    void SetUp() override
    {
        testfile.reset(new TestFile());
    }

public:
    TestFile::Ptr testfile;
};


std::string get_json_member_names(Json::Value& data)
{
    std::stringstream fields;
    for (const auto& f : data.getMemberNames())
    {
        fields << f << ":";
    }
    return std::string(fields.str());
}


TEST_F(ConfigurationFile, setval_integer)
{
    ASSERT_FALSE(testfile->IsPresent("integer-option"))
        << "integer-option was unexpectedly present";
    testfile->SetValue("integer-option", "123");
    Json::Value data = testfile->Generate();
    std::string fields_exp{"int_opt:"};
    ASSERT_EQ(get_json_member_names(data), fields_exp)
        << "Unexpected JSON members found";

    ASSERT_TRUE(testfile->IsPresent("integer-option"))
        << "integer-option was unexpectedly NOT present";
}


TEST_F(ConfigurationFile, setval_string)
{
    ASSERT_FALSE(testfile->IsPresent("string-option"))
        << "string-option was unexpectedly present";
    testfile->SetValue("string-option", "test-value");
    Json::Value data = testfile->Generate();
    std::string fields_exp{"str_opt:"};
    ASSERT_EQ(get_json_member_names(data), fields_exp)
        << "Unexpected JSON members found";

    ASSERT_TRUE(testfile->IsPresent("string-option"))
        << "string-option was unexpectedly NOT present";
}


TEST_F(ConfigurationFile, setval_present)
{
    // All of these will be considered "present" - but value differs
    std::map<std::string, bool> testvals = {
            {"0",     false},
            {"1",     true},
            {"2",     false},
            {"no",    false},
            {"yes",   true},
            {"false", false},
            {"true",  true},
            {"abc",   false},
            {"-1",    false}
        };
    for (const auto& val : testvals)
    {
        testfile->SetValue("present-option", val.first);
        Json::Value parsed = testfile->Generate();

        EXPECT_TRUE(parsed["present_opt"].asBool() == val.second)
            << "Unexpected value for" << val.first << " returned: "
            << parsed["present_opt"].asString() << std::endl << parsed;
   }

    // Check for an not-present option
    Json::Value parsed = testfile->Generate();
    EXPECT_TRUE(parsed["not_present"].asString() == "")
        << "Unexpected value for 'not-present-option returned: "
        << parsed["not_present"].asString() << std::endl << parsed;
}


TEST_F(ConfigurationFile, load_non_existent_file)
{
    EXPECT_THROW(testfile->Load("/tmp/non-existent-unit-test-file.json"),
                 ConfigFileException);
    ASSERT_TRUE(testfile->empty()) << "Non-existing test file is not empty";
}


TEST_F(ConfigurationFile, load_empty)
{
    unlink("/tmp/empty-unit-test-file.json");
    std::ofstream efile("/tmp/empty-unit-test-file.json");
    testfile->Load("/tmp/empty-unit-test-file.json");
    ASSERT_TRUE(testfile->empty()) << "Empty file resulted in parsed data";
    unlink("/tmp/empty-unit-test-file.json");
}


TEST_F(ConfigurationFile, write_empty)
{
    // Ensure we don't have this file
    unlink("/tmp/empty-unit-test-file.json");

    // This should not write any file to the file system
    testfile->Save("/tmp/empty-unit-test-file.json");

    struct stat fs;
    ASSERT_EQ(stat("/tmp/empty-unit-test-file.json", &fs), 0)
        << "Empty configuration file created";
    ASSERT_EQ(fs.st_size, 0)
        << "Saved configuration file is not empty (0 bytes)";
    unlink("/tmp/empty-unit-test-file.json");
}


TEST_F(ConfigurationFile, write_all)
{
    testfile->SetValue("integer-option", "4567");
    testfile->SetValue("string-option", "Testing String Option");
    testfile->SetValue("present-option", "1");

    // Ensure we don't have the test file before we write
    unlink("/tmp/unit-test-config-parser-file-1.json");

    testfile->Save("/tmp/unit-test-config-parser-file-1.json");

    // Parse the written JSON file
    std::ifstream infile("/tmp/unit-test-config-parser-file-1.json");
    std::string line;
    std::stringstream buf;
    while (std::getline(infile, line))
    {
        buf << line << std::endl;
    }

    Json::Value data;
    buf >> data;

    Json::Value testdata = testfile->Generate();

    // Compare data found in the saved file with
    // what have in the test object
    for (const auto& f : data.getMemberNames())
    {
        EXPECT_EQ(testdata[f], data[f]);
    }

    // Double check that all fields in the testobject is present
    // in the saved field
    for (const auto& f : testdata.getMemberNames())
    {
        EXPECT_EQ(data[f], testdata[f]);
    }

    unlink("/tmp/unit-test-config-parser-file-1.json");
}

TEST_F(ConfigurationFile, getoptions)
{
    Json::Value testdata;
    testdata["present_opt"] = true;
    testdata["str_opt"] = std::string{"Moar testing"};
    testfile->Parse(testdata);

    std::stringstream opts;
    for (const auto& o : testfile->GetOptions())
    {
        opts << o << ":";
    }
    ASSERT_EQ(opts.str(), std::string{"string-option:present-option:"})
        << "Unexpected list of present options";
}

TEST_F(ConfigurationFile, getval)
{
    Json::Value testdata;
    testdata["int_opt"] = std::string{"857393"};
    testdata["str_opt"] = std::string{"Moar testing"};

    testfile->Parse(testdata);
    EXPECT_EQ(testfile->GetValue("integer-option"), std::string{"857393"})
        << "Incorrect integer value returned";
    EXPECT_EQ(testfile->GetValue("string-option"), std::string{"Moar testing"})
        << "Incorrect string value returned";
    EXPECT_THROW(testfile->GetValue("unknown"), OptionNotFound)
        << "OptionNotFound exception was expected";
    EXPECT_THROW(testfile->GetValue("present-option"), OptionNotPresent)
        << "OptionNotPresent exception was expected";

    testdata["present_opt"] = false;
    testfile->Parse(testdata);
    EXPECT_TRUE(testfile->IsPresent("present-option"))
        << "present_opt is set -> IsPresent() should have returned true";
    EXPECT_EQ(testfile->GetValue("present-option"), "false")
        << "An empty string is expected when 'present-option' is set";
}

TEST_F(ConfigurationFile, load_file_normal)
{
    Json::Value rawdata;

    rawdata["int_opt"] = std::string("9876");
    rawdata["str_opt"] = std::string("Another test string");
    rawdata["present_opt"] = true;

    std::ofstream rawfile("/tmp/unit-test-config-parser-file-2.json");
    rawfile << rawdata << std::endl;
    rawfile.close();

    testfile->Load("/tmp/unit-test-config-parser-file-2.json");
    EXPECT_TRUE(testfile->IsPresent("integer-option"))
        << "Missing int_opt entry";
    EXPECT_TRUE(testfile->IsPresent("string-option"))
        << "Missing str_opt entry";
    EXPECT_TRUE(testfile->IsPresent("present-option"))
        << "Missing present_opt entry";

    unlink("/tmp/unit-test-config-parser-file-2.json");
}


TEST_F(ConfigurationFile, load_file_additional)
{
    Json::Value rawdata;

    rawdata["int_opt"] = std::string("29437");
    rawdata["str_opt"] = std::string("Yet more testing");
    rawdata["present_opt"] = std::string("1");
    rawdata["unexpected"] = std::string("some value");

    std::ofstream rawfile("/tmp/unit-test-config-parser-file-3.json");
    rawfile << rawdata << std::endl;
    rawfile.close();

    testfile->Load("/tmp/unit-test-config-parser-file-3.json");
    EXPECT_TRUE(testfile->IsPresent("integer-option"))
        << "Missing int_opt entry";
    EXPECT_TRUE(testfile->IsPresent("string-option"))
        << "Missing str_opt entry";
    EXPECT_TRUE(testfile->IsPresent("present-option"))
        << "Missing present-option entry";
    EXPECT_FALSE(testfile->IsPresent("not-present-option"))
        << "The not-present-option is present";
    EXPECT_THROW(testfile->IsPresent("unexpected"), OptionNotFound)
        << "Unexpected field found";


    Json::Value parsed = testfile->Generate();
    auto members = parsed.getMemberNames();
    EXPECT_TRUE(std::find(members.begin(),
                          members.end(),
                          "int_opt") != members.end())
        << "Could not find 'int_opt' in generated JSON data";
    EXPECT_TRUE(std::find(members.begin(),
                          members.end(),
                          "str_opt") != members.end())
        << "Could not find 'str_opt' in generated JSON data";
    EXPECT_TRUE(std::find(members.begin(),
                          members.end(),
                          "present_opt") != members.end())
        << "Could not find 'present_opt' in generated JSON data";
    EXPECT_FALSE(std::find(members.begin(),
                           members.end(),
                           "unexpected") != members.end())
        << "Found 'unexpected' in generated JSON data";

    unlink("/tmp/unit-test-config-parser-file-3.json");
}

TEST_F(ConfigurationFile, check_exclusive_valid)
{
    Json::Value data1;
    data1["grp1opt1"] = "123";
    data1["grp2opt1"] = "Valid test 1";
    testfile->Parse(data1);
    testfile->CheckExclusiveOptions();
}

TEST_F(ConfigurationFile, check_exclusive_fail_1)
{
    Json::Value data1;
    data1["grp1opt1"] = "123";
    data1["grp1opt2"] = "456";
    testfile->Parse(data1);
    EXPECT_THROW(testfile->CheckExclusiveOptions(), ExclusiveOptionError);
}

TEST_F(ConfigurationFile, check_exclusive_fail_2)
{
    Json::Value data2;
    data2["grp2optB"] = "Test string 2";
    data2["grp2optC"] = false;
    testfile->Parse(data2);
    EXPECT_THROW(testfile->CheckExclusiveOptions(), ExclusiveOptionError);
}

TEST_F(ConfigurationFile, get_related_group)
{
    std::vector<std::string> related = testfile->GetRelatedExclusiveOptions("group-2-optA");
    ASSERT_EQ(related.size(), 2) << "Not all expected related options found";

    auto s1 = std::find(related.begin(), related.end(), "group-2-optA");
    ASSERT_TRUE(s1 == related.end())
        << "Found group-2-optA in group2, should not be there";

    auto s2= std::find(related.begin(), related.end(), "group-2-optB");
    EXPECT_FALSE(s2 == related.end())
        << "Did not locate group-2-optB in group2";

    auto s3= std::find(related.begin(), related.end(), "group-2-optC");
    EXPECT_FALSE(s3 == related.end())
        << "Did not locate grgrp2optCp2optC in group2";
}

TEST_F(ConfigurationFile, single_entry_present)
{
    unlink("/tmp/unit-test-config-parser-single-entry.json");
    testfile->SetValue("present-option", "1");
    testfile->Save("/tmp/unit-test-config-parser-single-entry.json");

    // First, validate that we only have a single entry in the saved
    // JSON file

    // Parse the written JSON file
    std::ifstream infile("/tmp/unit-test-config-parser-single-entry.json");
    std::string line;
    std::stringstream buf;
    while (std::getline(infile, line))
    {
        buf << line << std::endl;
    }
    infile.close();

    Json::Value data;
    buf >> data;

    std::string fields_exp{"present_opt:"};
    ASSERT_EQ(get_json_member_names(data), fields_exp)
        << "Unexpected JSON members found";

    // Now, remove that entry and save the file again
    testfile->UnsetOption("present-option");
    testfile->Save("/tmp/unit-test-config-parser-single-entry.json");

    // Validate that this config has now been deleted
    struct stat fs;
    ASSERT_EQ(stat("/tmp/unit-test-config-parser-single-entry.json", &fs), 0)
        << "Empty configuration file created";
    ASSERT_EQ(fs.st_size, 0)
        << "Saved configuration file is not empty (0 bytes)";
    unlink("/tmp/unit-test-config-parser-single-entry.json");
}

} // namespace unittests
