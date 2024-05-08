//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2022 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2022 - 2023  David Sommerseth <davids@openvpn.net>
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
#include "log/logtag.hpp"

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

    LogMetaDataValue mdi1("numeric", -31415);
    EXPECT_STREQ(mdi1.label.c_str(), "numeric");
    EXPECT_STREQ(mdi1.GetValue().c_str(), "-31415");
    EXPECT_FALSE(mdi1.skip);

    LogMetaDataValue mdi2("numeric2", 4032918, true);
    EXPECT_STREQ(mdi2.label.c_str(), "numeric2");
    EXPECT_STREQ(mdi2.GetValue().c_str(), "4032918");
    EXPECT_TRUE(mdi2.skip);


    LogTag::Ptr tag = LogTag::Create("dummysender", "dummyinterface");
    LogMetaDataValue mdv3("tag", tag);
    EXPECT_STREQ(mdv3.label.c_str(), "tag");
    EXPECT_STREQ(mdv3.GetValue().c_str(), tag->str().c_str());
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

    LogMetaDataValue::Ptr mdi1 = LogMetaDataValue::create("labelC",
                                                          -12345);
    EXPECT_STREQ(mdi1->label.c_str(), "labelC");
    EXPECT_STREQ(mdi1->GetValue().c_str(), "-12345");
    EXPECT_FALSE(mdi1->skip);

    LogMetaDataValue::Ptr mdi2 = LogMetaDataValue::create("labelD",
                                                          67890,
                                                          true);
    EXPECT_STREQ(mdi2->label.c_str(), "labelD");
    EXPECT_STREQ(mdi2->GetValue().c_str(), "67890");
    EXPECT_TRUE(mdi2->skip);

    LogTag::Ptr tag = LogTag::Create("dummysender", "dummyinterface");
    LogMetaDataValue::Ptr mdv3 = LogMetaDataValue::create("tag", tag);
    EXPECT_STREQ(mdv3->label.c_str(), "tag");
    EXPECT_STREQ(mdv3->GetValue().c_str(), tag->str().c_str());
}


TEST(LogMetaDataValue, op_stream_write)
{
    LogMetaDataValue::Ptr mdv1 = LogMetaDataValue::create("meta_labelA",
                                                          "ValueValue");
    std::stringstream s1;
    s1 << *mdv1;
    EXPECT_STREQ(s1.str().c_str(), "meta_labelA=ValueValue");

    LogMetaDataValue::Ptr mdv2 = LogMetaDataValue::create("meta_labelB",
                                                          "DataData",
                                                          true);
    std::stringstream s2;
    s2 << *mdv2;
    EXPECT_STREQ(s2.str().c_str(), "");

    LogTag::Ptr tag = LogTag::Create("dummysender", "dummyinterface");
    std::string chk = "tag=" + tag->str();
    LogMetaDataValue::Ptr mdv3 = LogMetaDataValue::create("tag", tag);
    std::stringstream s3;
    s3 << *mdv3;
    EXPECT_STREQ(s3.str().c_str(), chk.c_str());
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
    EXPECT_STREQ(lmd.GetMetaValue("label2", false, "___").c_str(), "value 2___");

    LogTag::Ptr tag = LogTag::Create("dummysender", "dummyinterface");
    std::string chk_noencap = tag->str(false) + " ";
    std::string chk_encap = tag->str(true) + " ";
    lmd.AddMeta("tag", tag);
    EXPECT_STREQ(lmd.GetMetaValue("tag").c_str(), chk_encap.c_str());
    EXPECT_STREQ(lmd.GetMetaValue("tag", false).c_str(), chk_noencap.c_str());
    EXPECT_STREQ(lmd.GetMetaValue("tag", true).c_str(), chk_encap.c_str());
}


TEST(LogMetaData, static_op_stream_write)
{
    LogMetaData lmd;

    lmd.AddMeta("label_1", "Value: 1");
    lmd.AddMeta("label_2", "SomeOtherValue");
    lmd.AddMeta("skipped_label", "Skipped Value", true);

    LogTag::Ptr tag = LogTag::Create("dummysender", "dummyinterface");
    lmd.AddMeta("logtag", tag);
    std::string chk = "label_1=Value: 1, label_2=SomeOtherValue, logtag=" + tag->str();
    std::stringstream s;
    s << lmd;
    EXPECT_STREQ(s.str().c_str(), chk.c_str());
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
    EXPECT_STREQ(lmd->GetMetaValue("label2", false, "==").c_str(), "value 2==");
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

    LogTag::Ptr tag = LogTag::Create("dummysender", "dummyinterface");
    orig.AddMeta("logtag", tag);

    LogMetaData copy = LogMetaData(orig);
    EXPECT_STREQ(orig.GetMetaValue("label_A").c_str(),
                 copy.GetMetaValue("label_A").c_str());
    EXPECT_STREQ(orig.GetMetaValue("label_skip").c_str(),
                 copy.GetMetaValue("label_skip").c_str());
    EXPECT_STREQ(orig.GetMetaValue("label_B").c_str(),
                 copy.GetMetaValue("label_B").c_str());
    EXPECT_STREQ(orig.GetMetaValue("logtag").c_str(),
                 copy.GetMetaValue("logtag").c_str());

    orig.clear();
    EXPECT_TRUE(orig.empty());
    EXPECT_FALSE(copy.empty());

    EXPECT_STREQ(copy.GetMetaValue("label_A", false, "").c_str(), "Value A");
    EXPECT_STREQ(copy.GetMetaValue("label_skip", false, "").c_str(), "Value skip");
    EXPECT_STREQ(copy.GetMetaValue("label_B", false, "").c_str(), "Value B");
    EXPECT_STREQ(copy.GetMetaValue("logtag", false, "").c_str(), tag->str(false).c_str());
    EXPECT_STREQ(copy.GetMetaValue("logtag", true, "").c_str(), tag->str().c_str());

    std::string chk = "label_A=Value A, label_B=Value B, logtag=" + tag->str();
    std::stringstream s;
    s << copy;
    EXPECT_STREQ(s.str().c_str(), chk.c_str());

    EXPECT_STRNE(orig.GetMetaValue("label_A").c_str(),
                 copy.GetMetaValue("label_A").c_str());
    EXPECT_STRNE(orig.GetMetaValue("label_skip").c_str(),
                 copy.GetMetaValue("label_skip").c_str());
    EXPECT_STRNE(orig.GetMetaValue("label_B").c_str(),
                 copy.GetMetaValue("label_B").c_str());
}


void compare_logmetadata_records(LogMetaData::Records set1, LogMetaData::Records set2)
{
    ASSERT_EQ(set1.size(), set2.size()) << "Mismatch in LogMetaData::Record set sizes";

    int idx = 0;
    for (const auto &r : set1)
    {
        EXPECT_STREQ(r.c_str(), set2[idx].c_str());
        ++idx;
    }
}

TEST(LogMetaData, GetMetaDataRecords)
{
    LogMetaData lmd;
    lmd.AddMeta("label_A", "Value A");
    lmd.AddMeta("label_skip", "Value skip", true);
    lmd.AddMeta("label_B", "Value B");

    LogTag::Ptr tag = LogTag::Create("dummysender", "dummyinterface");
    lmd.AddMeta("logtag", tag);

    compare_logmetadata_records(lmd.GetMetaDataRecords(),
                                {"label_A=Value A",
                                 "label_skip=Value skip",
                                 "label_B=Value B",
                                 "logtag=" + tag->str()});

    compare_logmetadata_records(lmd.GetMetaDataRecords(true),
                                {"LABEL_A=Value A",
                                 "LABEL_SKIP=Value skip",
                                 "LABEL_B=Value B",
                                 "LOGTAG=" + tag->str()});

    compare_logmetadata_records(lmd.GetMetaDataRecords(false, false),
                                {"label_A=Value A",
                                 "label_skip=Value skip",
                                 "label_B=Value B",
                                 "logtag=" + tag->str(false)});

    compare_logmetadata_records(lmd.GetMetaDataRecords(true, false),
                                {"LABEL_A=Value A",
                                 "LABEL_SKIP=Value skip",
                                 "LABEL_B=Value B",
                                 "LOGTAG=" + tag->str(false)});
}
} // namespace unittest
