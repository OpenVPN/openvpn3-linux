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


namespace {
/**
 *  Helper method preparing struct iovec records to be passed to
 *  via the sd_journal_sendv() function.
 *
 *  The JournaldWriter will prefix most of the log data with a tag.
 *  This tag can be a "journald field name" or just prefixing the
 *  already provided field name.
 *
 *  This function just receives these strings and returns a fresh
 *  struct iovec with the needed data.
 *
 *  @param tag    std::string containing the tag - may be an empty string
 *  @param data   std::string containing the data to be logged by journald
 *  @returns struct iovec record with all fields set accordingly
 *  @throws JournalWriterException if memory allocation fails
 */
static iovec prepare_journal_iov(const std::string &tag, const std::string &data)
{
    size_t buflen = tag.length() + data.length();
    char *data_c = static_cast<char *>(calloc(buflen, 1));
    if (data_c)
    {
        if (!tag.empty())
        {
            memcpy(data_c, tag.c_str(), tag.length());
        }
        memcpy(data_c + tag.length(), data.c_str(), data.length());
        return iovec{data_c, buflen};
    }
    throw JournalWriterException("Error allocating memory for '" + tag + data + "'");
}
} // namespace



JournalWriterException::JournalWriterException(const std::string &err) noexcept
    : error(err)
{
}

const char *JournalWriterException::what() const noexcept
{
    return error.c_str();
}



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
    struct iovec *l = static_cast<struct iovec *>(calloc(sizeof(struct iovec) + 2,
                                                         meta_size));
    if (!l)
    {
        throw JournalWriterException("Failed to allocate data for log event: "
                                     + event.str(10, true));
    }

    // Add the fixed O3_LOG_SENDER data, to more easily identify
    // log events from this log service across all Linux distros in the journal
    size_t i = 0;
    l[i++] = prepare_journal_iov("", log_sender);

    if (metadata)
    {
        for (const auto &mdr : metadata->GetMetaDataRecords(true, false))
        {
            l[i++] = prepare_journal_iov("O3_", mdr);
        }
    }

    auto logtag = event.GetLogTag();
    if (logtag)
    {
        l[i++] = prepare_journal_iov("O3_LOGTAG=", logtag->str(false));
    }

    if (!event.session_token.empty())
    {
        l[i++] = prepare_journal_iov("O3_SESSION_TOKEN=", event.session_token);
    }

    l[i++] = prepare_journal_iov("O3_LOG_GROUP=", event.GetLogGroupStr());
    l[i++] = prepare_journal_iov("O3_LOG_CATEGORY=", event.GetLogCategoryStr());

    std::string msg;
    if (prepend_prefix && logtag)
    {
        msg += logtag->str(true) + " ";
    }
    msg += event.message;
    l[i++] = prepare_journal_iov("MESSAGE=", msg);

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
