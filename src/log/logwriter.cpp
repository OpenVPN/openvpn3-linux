//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2022  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2022  David Sommerseth <davids@openvpn.net>
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
 * @file   logwriter.cpp
 *
 * @brief  Base class implementing a generic logging interface (API)
 *         as well as file stream and syslog implementations.
 */


#include <sys/uio.h>
#include <ctype.h>

#include <algorithm>
#include <fstream>
#include <exception>
#include <string>
#include <vector>

#define SD_JOURNAL_SUPPRESS_LOCATION
#include <systemd/sd-journal.h>

#include "logwriter.hpp"


//
//  LogMetaDataValue  -  implementation
//

LogMetaDataValue::LogMetaDataValue(const std::string& l, const std::string& v,
                                   bool s)
    : label(l), str_value(v), logtag((const LogTag&)LogTag()), skip(s)
{
    type = Type::LOGMETA_STRING;
}


LogMetaDataValue::LogMetaDataValue(const std::string& l, const LogTag& v,
                                   bool s)
    : label(l), str_value(""), logtag(v), skip(s)
{
    type = Type::LOGMETA_LOGTAG;
}

const std::string LogMetaDataValue::GetValue(const bool logtag_encaps) const
{
    switch(type)
    {
    case Type::LOGMETA_STRING:
        return str_value;

    case Type::LOGMETA_LOGTAG:
        return logtag.str(logtag_encaps);
    }
    return "";
}


//
//  LogMetaData  -  implementation
//

std::string LogMetaData::GetMetaValue(const std::string l,
                                      const bool encaps_logtag,
                                      const std::string postfix) const
{
    auto it = std::find_if(metadata.begin(), metadata.end(),
                           [l](LogMetaDataValue::Ptr e)
                           {
                               return l == e->label;
                           });
    if (metadata.end() == it)
    {
        return "";
    }
    return (*it)->GetValue(encaps_logtag) + postfix;
}


LogMetaData::Records LogMetaData::GetMetaDataRecords(const bool upcase_label,
                                        const bool logtag_encaps) const
{
    Records ret;
    for (const auto& mdc : metadata)
    {
        std::string label = mdc->label;
        if (upcase_label)
        {
            std::transform(label.begin(), label.end(), label.begin(),
                           [](unsigned char c){ return std::toupper(c); });
        }
        if (LogMetaDataValue::Type::LOGMETA_LOGTAG == mdc->type)
        {
            ret.push_back(label + "=" + mdc->GetValue(logtag_encaps));
        }
        else
        {
            ret.push_back(label + "=" + mdc->GetValue());
        }
    }
    return ret;
}


size_t LogMetaData::size() const
{
    return metadata.size();
}


bool LogMetaData::empty() const
{
    return metadata.empty();
}


void LogMetaData::clear()
{
    metadata.clear();
}


//
//  JournaldWriter - implementation
//
JournaldWriter::JournaldWriter()
    : LogWriter()
{
}


bool JournaldWriter::TimestampEnabled()
{
    return true;
}


void JournaldWriter::Write(const std::string& data,
                           const std::string& colour_init,
                           const std::string& colour_reset)
{
    JournaldWriter::Write(LogEvent(LogGroup::UNDEFINED, LogCategory::INFO,
                                   data));
}


void JournaldWriter::Write(const LogGroup grp, const LogCategory ctg,
           const std::string& data,
           const std::string& colour_init,
           const std::string& colour_reset)
{
    JournaldWriter::Write(LogEvent(grp, ctg, data));
}

void JournaldWriter::Write(const LogEvent& event)
{
    // We need extra elements for O3_SESSION_TOKEN,
    // O3_LOG_GROUP, O3_LOG_CATEGORY, MESSAGE and
    // the NULL termination
    struct iovec *l = (struct iovec *) calloc(sizeof(struct iovec)+2,
                                              metadata.size()+6);
    size_t i = 0;
    for (const auto& r : metadata.GetMetaDataRecords(true, false))
    {
        std::string m = std::string("O3_") + r;
        l[i++] = {(char *) strdup(m.c_str()), m.length()};
    }

    std::string st("O3_SESSION_TOKEN=");
    if (!event.session_token.empty())
    {
        st += event.session_token;
        l[i++] = {(char *) strdup(st.c_str()), st.length()};
    }

    std::string lg("O3_LOG_GROUP=" + event.GetLogGroupStr());
    l[i++] = {(char *) strdup(lg.c_str()), lg.length()};

    std::string lc("O3_LOG_CATEGORY=" + event.GetLogCategoryStr());
    l[i++] = {(char *) strdup(lc.c_str()), lc.length()};

    std::string m("MESSAGE=");
    if (prepend_prefix && prepend_meta)
    {
        m += metadata.GetMetaValue(prepend_label, true);
    }

    m += event.message;
    l[i++] = {(char *) strdup(m.c_str()), m.length()};
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

    prepend_label.clear();
    metadata.clear();
}
