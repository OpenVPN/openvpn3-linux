//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2022         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2022         David Sommerseth <davids@openvpn.net>
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
 * @file   logmetadata.cpp
 *
 * @brief  Unit test for LogMetaData and LogMetaDataValue classes
 */

#include <iostream>
#include <string>
#include <sstream>

#include <gtest/gtest.h>

#include "log/logwriter.hpp"

namespace unittest {

TEST(LogMetaDataValue, constructor)
{
    LogMetaDataValue mdv1("label1", "value");
    EXPECT_STREQ(mdv1.label.c_str(), "label1");
    EXPECT_STREQ(mdv1.GetValue().c_str(), "value");
    EXPECT_FALSE(mdv1.skip);

    LogMetaDataValue mdv2("label2", "another value", true);
    EXPECT_STREQ(mdv2.label.c_str(), "label2");
    EXPECT_STREQ(mdv2.GetValue().c_str(), "another value");
    EXPECT_TRUE(mdv2.skip);
}


TEST(LogMetaDataValue, create)
{
    LogMetaDataValue::Ptr mdv1 = LogMetaDataValue::create("labelA",
                                                          "valueA");
    EXPECT_STREQ(mdv1->label.c_str(), "labelA");
    EXPECT_STREQ(mdv1->GetValue().c_str(), "valueA");
    EXPECT_FALSE(mdv1->skip);

    LogMetaDataValue::Ptr mdv2 = LogMetaDataValue::create("labelB",
                                                          "A different value",
                                                          true);
    EXPECT_STREQ(mdv2->label.c_str(), "labelB");
    EXPECT_STREQ(mdv2->GetValue().c_str(), "A different value");
    EXPECT_TRUE(mdv2->skip);
}


TEST(LogMetaDataValue, op_stream_write)
{
    LogMetaDataValue::Ptr mdv1 = LogMetaDataValue::create("meta_labelA",
                                                          "ValueValue");
    std::stringstream s1;
    s1 << *mdv1;
    EXPECT_STREQ(s1.str().c_str(), "meta_labelA=ValueValue");

    LogMetaDataValue::Ptr mdv2 = LogMetaDataValue::create("meta_labelB",
                                                          "DataData", true);
    std::stringstream s2;
    s2 << *mdv2;
    EXPECT_STREQ(s2.str().c_str(), "");
}


TEST(LogMetaData, static_empty)
{
    LogMetaData lmd;
    EXPECT_TRUE(lmd.empty());

    lmd.AddMeta("someLabel", "someValue");
    EXPECT_FALSE(lmd.empty());

    lmd.clear();
    EXPECT_TRUE(lmd.empty()) << "Clearing failed";
}


TEST(LogMetaData, static_add_get_meta)
{
    LogMetaData lmd;

    // Note: GetMetaValue() adds a " " postfix by default
    lmd.AddMeta("label1", "value 1");
    EXPECT_STREQ(lmd.GetMetaValue("label1").c_str(), "value 1 ") << " ## postfix not set";
    EXPECT_STREQ(lmd.GetMetaValue("label1", false, "").c_str(), "value 1") << " ## postfix=''";
    EXPECT_STREQ(lmd.GetMetaValue("label1", false, "___").c_str(), "value 1___") << " ## postfix='___'";

    lmd.AddMeta("label2", "value 2", true);
    EXPECT_STREQ(lmd.GetMetaValue("label2").c_str(), "value 2 ");
    EXPECT_STREQ(lmd.GetMetaValue("label2", false,"___").c_str(), "value 2___");
}


TEST(LogMetaData, static_op_stream_write)
{
    LogMetaData lmd;

    lmd.AddMeta("label_1", "Value: 1");
    lmd.AddMeta("label_2", "SomeOtherValue");
    lmd.AddMeta("skipped_label", "Skipped Value", true);
    std::stringstream s;
    s << lmd;
    EXPECT_STREQ(s.str().c_str(), "label_1=Value: 1, label_2=SomeOtherValue");
}


TEST(LogMetaData, ptr_empty)
{
    LogMetaData::Ptr lmd = LogMetaData::create();
    EXPECT_TRUE(lmd->empty());

    lmd->AddMeta("someLabel", "someValue");
    EXPECT_FALSE(lmd->empty());

    lmd->clear();
    EXPECT_TRUE(lmd->empty()) << "Clearing failed";
}


TEST(LogMetaData, ptr_add_get_meta)
{
    LogMetaData::Ptr lmd = LogMetaData::create();

    // Note: GetMetaValue() adds a " " postfix by default
    lmd->AddMeta("label1", "value 1");
    EXPECT_STREQ(lmd->GetMetaValue("label1").c_str(), "value 1 ");
    EXPECT_STREQ(lmd->GetMetaValue("label1", false, "").c_str(), "value 1");
    EXPECT_STREQ(lmd->GetMetaValue("label1", false, "==").c_str(), "value 1==");

    lmd->AddMeta("label2", "value 2", true);
    EXPECT_STREQ(lmd->GetMetaValue("label2").c_str(), "value 2 ");
    EXPECT_STREQ(lmd->GetMetaValue("label2", false,"==").c_str(), "value 2==");
}


TEST(LogMetaData, ptr_op_stream_write)
{
    LogMetaData::Ptr lmd = LogMetaData::create();

    lmd->AddMeta("label_1", "Value: 1");
    lmd->AddMeta("label_2", "SomeOtherValue");
    lmd->AddMeta("skipped_label", "Skipped Value", true);
    std::stringstream s;
    s << *lmd;
    EXPECT_STREQ(s.str().c_str(), "label_1=Value: 1, label_2=SomeOtherValue");
}


TEST(LogMetaData, op_copy)
{
    // This test might seem superfluous as it is more tests C++ basics
    // but since we use this copy approach in logwriter.hpp, lets ensure
    // it behaves as expected anyhow.

    LogMetaData orig;
    orig.AddMeta("label_A", "Value A");
    orig.AddMeta("label_skip", "Value skip", true);
    orig.AddMeta("label_B", "Value B");

    LogMetaData copy = LogMetaData(orig);
    EXPECT_STREQ(orig.GetMetaValue("label_A").c_str(),
                 copy.GetMetaValue("label_A").c_str());
    EXPECT_STREQ(orig.GetMetaValue("label_skip").c_str(),
                 copy.GetMetaValue("label_skip").c_str());
    EXPECT_STREQ(orig.GetMetaValue("label_B").c_str(),
                 copy.GetMetaValue("label_B").c_str());

    orig.clear();
    EXPECT_TRUE(orig.empty());
    EXPECT_FALSE(copy.empty());

    EXPECT_STREQ(copy.GetMetaValue("label_A", false, "").c_str(), "Value A");
    EXPECT_STREQ(copy.GetMetaValue("label_skip", false, "").c_str(), "Value skip");
    EXPECT_STREQ(copy.GetMetaValue("label_B", false, "").c_str(), "Value B");

    std::stringstream s;
    s << copy;
    EXPECT_STREQ(s.str().c_str(), "label_A=Value A, label_B=Value B");

    EXPECT_STRNE(orig.GetMetaValue("label_A").c_str(),
                 copy.GetMetaValue("label_A").c_str());
    EXPECT_STRNE(orig.GetMetaValue("label_skip").c_str(),
                 copy.GetMetaValue("label_skip").c_str());
    EXPECT_STRNE(orig.GetMetaValue("label_B").c_str(),
                 copy.GetMetaValue("label_B").c_str());
}
} // unittest
