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



JournaldWriter::JournaldWriter(const std::string &logsndr)
    : LogWriter(), log_sender("O3_LOG_SENDER=" + logsndr)
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


void JournaldWriter::WriteLogLine(LogTag::Ptr logtag,
                                  const std::string &data,
                                  const std::string &colour_init,
                                  const std::string &colour_reset)
{
    Events::Log logev(LogGroup::UNDEFINED, LogCategory::INFO, data);
    logev.AddLogTag(logtag);
    JournaldWriter::Write(logev);
}


void JournaldWriter::WriteLogLine(LogTag::Ptr logtag,
                                  const LogGroup grp,
                                  const LogCategory ctg,
                                  const std::string &data,
                                  const std::string &colour_init,
                                  const std::string &colour_reset)
{
    Events::Log logev(grp, ctg, data);
    logev.AddLogTag(logtag);
    JournaldWriter::Write(logev);
}


void JournaldWriter::Write(const Events::Log &event)
{
    // We need extra elements for O3_LOGTAG, O3_SESSION_TOKEN,
    // O3_LOG_GROUP, O3_LOG_CATEGORY, MESSAGE and
    // the NULL termination
    size_t meta_size = (metadata ? metadata->size() : 0) + 8;
    struct iovec *l = (struct iovec *)calloc(sizeof(struct iovec) + 2,
                                             meta_size);

    // Add the fixed O3_LOG_SENDER data, to more easily identify
    // log events from this log service across all Linux distros in the journal
    size_t i = 0;
    l[i++] = {(char *)strdup(log_sender.c_str()), log_sender.length()};

    if (metadata)
    {
        for (const auto &mdr : metadata->GetMetaDataRecords(true, false))
        {
            std::string md = std::string("O3_") + mdr;
            l[i++] = {(char *)strdup(md.c_str()), md.length()};
        }
    }

    auto logtag = event.GetLogTag();
    std::string logtag_str("O3_LOGTAG=");
    if (logtag)
    {
        logtag_str += logtag->str(false);
        l[i++] = {(char *)strdup(logtag_str.c_str()), logtag_str.length()};
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
    if (prepend_prefix && logtag)
    {
        m += logtag->str(true) + " ";
    }
    m += event.GetLogGroupStr() + ": ";

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
