//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   attention-req.cpp
 *
 * @brief  Unit test for struct AttentionReq
 */

#include <iostream>
#include <string>
#include <sstream>

#include <gtest/gtest.h>

#include "events/attention-req.hpp"


namespace unittest {

std::string test_empty(const Events::AttentionReq &ev, const bool expect)
{
    bool r = ev.empty();
    if (expect != r)
    {
        return std::string("test_empty():  ")
               + "ev.empty() = " + (r ? "true" : "false")
               + " [expected: " + (expect ? "true" : "false") + "]";
    }

    r = (ClientAttentionType::UNSET == ev.type
         && ClientAttentionGroup::UNSET == ev.group
         && ev.Message().empty());
    if (expect != r)
    {
        return std::string("test_empty() - Member check:  ")
               + "(" + std::to_string(static_cast<uint32_t>(ev.group)) + ", "
               + std::to_string(static_cast<uint32_t>(ev.group)) + "', "
               + "message.size=" + std::to_string(ev.message.size()) + ") ..."
               + " is " + (r ? "EMPTY" : "NON-EMPTY")
               + " [expected: " + (expect ? "EMPTY" : "NON-EMPTY") + "]";
    }
    return "";
};


TEST(AttentionReq, init_empty)
{
    Events::AttentionReq empty;
    std::string res = test_empty(empty, true);
    EXPECT_TRUE(res.empty()) << res;
    EXPECT_TRUE(empty.empty()) << res;
}


TEST(AttentionReq, init_with_values)
{
    Events::AttentionReq populated1(ClientAttentionType::CREDENTIALS,
                                    ClientAttentionGroup::CHALLENGE_AUTH_PENDING,
                                    "Test attention 1");
    std::string res = test_empty(populated1, false);
    EXPECT_TRUE(res.empty()) << res;
    EXPECT_FALSE(populated1.empty()) << res;
}


TEST(AttentionReq, reset)
{
    Events::AttentionReq populated1(ClientAttentionType::CREDENTIALS,
                                    ClientAttentionGroup::OPEN_URL,
                                    "Test attention 2");
    populated1.reset();
    std::string res = test_empty(populated1, true);
    EXPECT_TRUE(res.empty()) << res;
    EXPECT_TRUE(populated1.empty()) << res;
}


TEST(AttentionReq, parse_gvariant_invalid_data)
{
    GVariant *data = nullptr;
    data = g_variant_new("(uu)", 1, 2);

    EXPECT_THROW(Events::AttentionReq parsed(data),
                 DBus::Exception);
    if (nullptr != data)
    {
        g_variant_unref(data);
    }
}


TEST(AttentionReq, parse_gvariant_valid_tuple)
{
    GVariant *data = g_variant_new("(uus)",
                                   static_cast<uint32_t>(ClientAttentionType::CREDENTIALS),
                                   static_cast<uint32_t>(ClientAttentionGroup::USER_PASSWORD),
                                   "Parse testing again");
    Events::AttentionReq parsed(data);
    EXPECT_EQ(parsed.type, ClientAttentionType::CREDENTIALS);
    EXPECT_EQ(parsed.group, ClientAttentionGroup::USER_PASSWORD);
    EXPECT_EQ(parsed.message, "Parse testing again");

    g_variant_unref(data);
}


TEST(AttentionReq, GetGVariantTuple)
{
    Events::AttentionReq reverse(ClientAttentionType::CREDENTIALS,
                                 ClientAttentionGroup::CHALLENGE_STATIC,
                                 "Yet another test");
    GVariant *revparse = reverse.GetGVariant();
    glib2::Utils::checkParams(__func__, revparse, "(uus)", 3);

    auto type = glib2::Value::Extract<ClientAttentionType>(revparse, 0);
    auto group = glib2::Value::Extract<ClientAttentionGroup>(revparse, 1);
    auto msg = glib2::Value::Extract<std::string>(revparse, 2);

    EXPECT_EQ(group, reverse.group);
    EXPECT_EQ(type, reverse.type);
    EXPECT_EQ(msg, reverse.message);

    g_variant_unref(revparse);
}


TEST(AttentionReq, stringstream)
{
    Events::AttentionReq status(ClientAttentionType::CREDENTIALS,
                                ClientAttentionGroup::PK_PASSPHRASE,
                                "Private Key passphrase");

    std::stringstream chk;
    chk << status;
    std::string expect("AttentionReq(User Credentials, Private key passphrase): "
                       "Private Key passphrase");
    EXPECT_EQ(chk.str(), expect);
}


} // namespace unittest
