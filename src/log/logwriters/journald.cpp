//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//  Copyright (C)  Raphael Mader <mader.raphael@gmx.at>
//

/**
 * @file   journald.cpp
 *
 * @brief  Implementation of JournaldWriter
 */

#include "build-config.h"

#ifdef HAVE_SYSTEMD

#include <sys/uio.h>
#include <ctype.h>
#include <string>

#define SD_JOURNAL_SUPPRESS_LOCATION
#include <systemd/sd-journal.h>

#include "log/logwriter.hpp"
#include "log/logwriters/journald.hpp"



JournaldWriter::JournaldWriter()
    : LogWriter()
{
}


const std::string JournaldWriter::GetLogWriterInfo() const
{
    return std::string("journald");
}


bool JournaldWriter::TimestampEnabled()
{
    return true;
}


void JournaldWriter::Write(const std::string &data,
                           const std::string &colour_init,
                           const std::string &colour_reset)
{
    JournaldWriter::Write(Events::Log(LogGroup::UNDEFINED,
                                      LogCategory::INFO,
                                      data));
}


void JournaldWriter::Write(const LogGroup grp,
                           const LogCategory ctg,
                           const std::string &data,
                           const std::string &colour_init,
                           const std::string &colour_reset)
{
    JournaldWriter::Write(Events::Log(grp, ctg, data));
}


void JournaldWriter::Write(const Events::Log &event)
{
    // We need extra elements for O3_SESSION_TOKEN,
    // O3_LOG_GROUP, O3_LOG_CATEGORY, MESSAGE and
    // the NULL termination
    struct iovec *l = (struct iovec *)calloc(sizeof(struct iovec) + 2,
                                             metadata->size() + 6);
    size_t i = 0;
    if (metadata)
    {
        for (const auto &mdr : metadata->GetMetaDataRecords(true, false))
        {
            std::string md = std::string("O3_") + mdr;
            l[i++] = {(char *)strdup(md.c_str()), md.length()};
        }
    }

    std::string st("O3_SESSION_TOKEN=");
    if (!event.session_token.empty())
    {
        st += event.session_token;
        l[i++] = {(char *)strdup(st.c_str()), st.length()};
    }

    std::string lg("O3_LOG_GROUP=" + event.GetLogGroupStr());
    l[i++] = {(char *)strdup(lg.c_str()), lg.length()};

    std::string lc("O3_LOG_CATEGORY=" + event.GetLogCategoryStr());
    l[i++] = {(char *)strdup(lc.c_str()), lc.length()};

    std::string m("MESSAGE=");

    m += event.message;
    l[i++] = {(char *)strdup(m.c_str()), m.length()};
    l[i] = {NULL};

    int r = sd_journal_sendv(l, i);
    if (0 != r)
    {
        std::cout << "ERROR: " << strerror(errno) << std::endl;
    }

    // struct iovec is a C interface; need to clean up manually
    for (size_t j = 0; j < i; j++)
    {
        free(l[j].iov_base);
        l[j].iov_base = 0;
        l[j].iov_len = 0;
    }
    free(l);

    if (metadata)
    {
        metadata->clear();
    }
}
#endif // HAVE_SYSTEMD
