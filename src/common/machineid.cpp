//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2021         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2021         David Sommerseth <davids@openvpn.net>
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
 * @file   machineid.cpp
 *
 * @brief  Generate a static unique machine ID
 */

#include "config.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <uuid/uuid.h>

#ifdef HAVE_SYSTEMD
#include <systemd/sd-id128.h>
#endif

#ifdef USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openvpn/openssl/compat.hpp>
#endif

#include "machineid.hpp"

MachineIDException::MachineIDException(const std::string& msg) noexcept
{
    error = std::string(msg);
}

std::string MachineIDException::GetError() const noexcept
{
    return error;
}

const char* MachineIDException::what()
{
    return error.c_str();
}

MachineID::MachineID(const std::string& local_machineid, bool enforce_local)
{

    std::string rawid;

#if HAVE_SYSTEMD
    sd_id128_t sdmid;
    if (!enforce_local && (0 == sd_id128_get_machine(&sdmid)))
    {
        char sdmidstr[SD_ID128_STRING_MAX];
        rawid = std::string(sd_id128_to_string(sdmid, sdmidstr));
        source = MachineID::SourceType::SYSTEMD_API;
    }
    else
#endif
    {
        std::vector<std::string> files;
        if (!enforce_local)
        {
            files.push_back("/etc/machine-id");
        }
        files.push_back(local_machineid);

        std::ifstream machineid_file;
        bool success = false;
        for (const auto& fname : files)
        {
            machineid_file.open(fname);
            if (!machineid_file.eof() && !machineid_file.fail())
            {
                success = true;
                if ((fname == local_machineid)
                    && ("/etc/machine-id" != local_machineid))
                {
                    source = MachineID::SourceType::LOCAL;
                }
                else
                {
                    source = MachineID::SourceType::SYSTEM;
                }
                break;
            }
        }

        if (success)
        {
            std::getline(machineid_file, rawid);
        }
        else
        {
            // No machine ID was found; generate one on-the-fly and save it
            rawid = generate_machine_id(local_machineid);
            if (errormsg.empty())
            {
                source = MachineID::SourceType::LOCAL;
            }
            else
            {
                // If an error message appeared in generate_machine_id, the
                // source is completely random
                source = MachineID::SourceType::RANDOM;
            }
        }
    }

    try
    {
#ifdef USE_OPENSSL
        // This is only supported when building with OpenSSL.
        // Ideally we should openvpn::HashString or a similar
        // implementation, but that code does not like being
        // used in multiple build units. So we bail-out for
        // now with with a simple direct OpenSSL
        // implementation.

        // Initialise an OpenSSL message digest context for SHA256
        const EVP_MD *md = EVP_sha256();
        if (nullptr == md)
        {
            throw MachineIDException("[OpenSSL] Failed preparing SHA256 MD context");
        }
        EVP_MD_CTX *ctx = EVP_MD_CTX_new();
        if (nullptr == ctx)
        {
            throw MachineIDException("[OpenSSL] Failed creating hash context");
        }

        // The raw ID cannot be used directly; see man machine-id(5)
        // So we hard-code some fixed values for OpenVPN 3 Linux together
        // with this raw machine-id and use that value for the hashing.
        EVP_DigestInit_ex(ctx, md, nullptr);
        EVP_DigestUpdate(ctx, "OpenVPN 3", 9);
        EVP_DigestUpdate(ctx, rawid.c_str(), rawid.length());
        EVP_DigestUpdate(ctx, "Linux", 5);

        // Calculate the SHA256 hash of the date
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int len = 0;
        EVP_DigestFinal_ex(ctx, hash, &len);
        EVP_MD_CTX_free(ctx);

        // Format the calculated hash as a readable hex string
        std::stringstream output;
        for(unsigned int i = 0; i < len; i++) {
                output << std::setw(2) << std::setfill('0')
                       << std::hex << (int) hash[i];
        }
        machine_id = std::string(output.str());
#else
        errormsg = "Retrieving a MachineID is only supported with OpenSSL";
        source = MachineID::SourceType::NONE;
#endif
    }
    catch (const std::exception& excp)
    {
        throw MachineIDException(std::string("Failed to generate MachineID: ")
                                  + std::string(excp.what()));
    }
}


void MachineID::success() const
{
    if (!errormsg.empty())
    {
        throw MachineIDException(errormsg);
    }
}


MachineID::SourceType MachineID::GetSource() const noexcept
{
    return source;
}


std::string MachineID::get() const noexcept
{
    return machine_id;
}


std::string MachineID::generate_machine_id(const std::string& fname) noexcept
{
    uuid_t uuid_bin;
    uuid_generate_time_safe(uuid_bin);

    char uuid_str[40];
    uuid_unparse_lower(uuid_bin, uuid_str);

    std::string uuid{uuid_str};

    std::ofstream machineid(fname);
    machineid << uuid << std::endl;
    machineid.close();
    if (machineid.fail())
    {
        errormsg = "Could not save the auto-generated machine-id to '"
                   + fname + "'";
    }

    return uuid;
}
