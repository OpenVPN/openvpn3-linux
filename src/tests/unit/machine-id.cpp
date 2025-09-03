//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   machine-id.cpp
 *
 * @brief  Unit test for MachineID
 */

#include "build-config.h"

#include <iomanip>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <gtest/gtest.h>

#include "common/machineid.hpp"

#ifndef USE_OPENSSL
namespace unittest {
TEST(MachineID, not_implemented)
{
    GTEST_SKIP() << "MachineID tests are only available with OpenSSL builds";
}
} // namespace unittest
#else

#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openvpn/openssl/compat.hpp>

namespace unittest {
class ReferenceID
{
  public:
    ReferenceID(const std::string &filename)
    {
        uuid_t id_bin;
        uuid_generate_random(id_bin);

        char uuid_str[40];
        uuid_unparse_lower(id_bin, uuid_str);

        std::ofstream machineid_file(filename);
        machineid_file << std::string(uuid_str) << std::endl;
        machineid_file.close();
        if (machineid_file.fail())
        {
            throw MachineIDException(std::string("Could not save generated ")
                                     + "machine-id file '"
                                     + std::string(filename) + "'");
        }

        refid = hash_sha256(std::string("OpenVPN 3") + std::string(uuid_str)
                            + std::string("Linux"));
    }

    std::string get() const noexcept
    {
        return refid;
    }


  private:
    std::string refid{};

    std::string hash_sha256(const std::string &input)
    {
        // Initialise an OpenSSL message digest context for SHA256
        const EVP_MD *md = EVP_sha256();
        assert(nullptr != md);
        EVP_MD_CTX *ctx = EVP_MD_CTX_new();
        assert(nullptr != ctx);

        // Pass data to be hashed
        EVP_DigestInit_ex(ctx, md, nullptr);
        EVP_DigestUpdate(ctx, input.c_str(), input.length());

        // Calculate the SHA256 hash of the date
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int len = 0;
        EVP_DigestFinal_ex(ctx, hash, &len);
        EVP_MD_CTX_free(ctx);

        // Format the calculated hash as a readable hex string
        std::stringstream output;
        for (unsigned int i = 0; i < len; i++)
        {
            output << std::setw(2) << std::setfill('0')
                   << std::hex << (int)hash[i];
        }
        return std::string(output.str());
    }
};

class MachineIDTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        try
        {
            refid.reset(new ReferenceID("unit-test_machine-id"));
        }
        catch (MachineIDException &excp)
        {
            FAIL() << excp.what();
        }
    }

    void TearDown() override
    {
        ::unlink("unit-test_machine-id");
    }

  public:
    std::unique_ptr<ReferenceID> refid;

  private:
};


TEST_F(MachineIDTest, get_system)
{
    std::ifstream sys_machineid_file("/etc/machine-id");
    std::string sysid;
    sys_machineid_file >> sysid;
    if (!sys_machineid_file.fail())
    {
        MachineID machid("/etc/machine-id", true);
        EXPECT_TRUE(machid.GetSource() == MachineID::SourceType::SYSTEM);
    }
    else
    {
        GTEST_SKIP() << "Missing /etc/system-id file";
    }
    sys_machineid_file.close();
}

TEST_F(MachineIDTest, get_systemd_api)
{
#ifdef HAVE_SYSTEMD
    MachineID::SourceType expect_src = MachineID::SourceType::SYSTEMD_API;
    std::string note{};

    if (0 == access("/etc/machine-id", R_OK))
    {
        struct stat s;
        if (0 == stat("/etc/machine-id", &s))
        {
            if (1 > s.st_size)
            {
                expect_src = MachineID::SourceType::SYSTEM;
                note = std::string("/etc/machine-id was empty, ")
                       + "expecting SourceType::SYSTEM";
            }
        }
        else
        {
            expect_src = MachineID::SourceType::SYSTEM;
            note = std::string("/etc/machine-id was inaccessible, ")
                   + "expecting SourceType::SYSTEM";
        }
    }
    else
    {
        expect_src = MachineID::SourceType::SYSTEM;
        note = std::string("/etc/machine-id was inaccessible, ")
               + "expecting SourceType::SYSTEM";
    }

    MachineID machid;
    EXPECT_TRUE(machid.GetSource() == expect_src);
    if (!note.empty())
    {
        std::cout << "MachineIDTest::get_systemd_api: *NOTE* "
                  << note << std::endl;
    }
#else
    {
        GTEST_SKIP() << "Needed systemd API not available or unusable";
    }
#endif
}

TEST_F(MachineIDTest, get)
{
    MachineID machid("unit-test_machine-id", true);
    EXPECT_TRUE(machid.GetSource() == MachineID::SourceType::LOCAL);
    ASSERT_NO_THROW(machid.success());
    ASSERT_EQ(machid.get(), refid->get());
}

TEST_F(MachineIDTest, stringstream)
{
    MachineID machid("unit-test_machine-id", true);
    EXPECT_TRUE(machid.GetSource() == MachineID::SourceType::LOCAL);
    ASSERT_NO_THROW(machid.success());

    std::stringstream id;
    id << machid;
    ASSERT_EQ(id.str(), refid->get());
}

TEST_F(MachineIDTest, fail_machine_id_save)
{
    MachineID machid("/some/non/existing/path/ovpn3-unittest-machine-id", true);
    EXPECT_TRUE(machid.GetSource() == MachineID::SourceType::RANDOM);
    ASSERT_THROW(machid.success(), MachineIDException);
}

} // namespace unittest
#endif // USE_OPENSSL
