//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   streamwriter.cpp
 *
 * @brief  Implementation of StreamLogWriter and ColourStreamWriter
 */

#include <string>

#include "common/timestamp.hpp"
#include "../logwriter.hpp"
#include "streamwriter.hpp"


//
//  StreamLogWriter - implementation
//

StreamLogWriter::StreamLogWriter(std::ostream &dst)
    : LogWriter(), dest(dst)
{
}

StreamLogWriter::~StreamLogWriter()
{
    dest.flush();
}


const std::string StreamLogWriter::GetLogWriterInfo() const
{
    return std::string("StreamWriter");
}


void StreamLogWriter::WriteLogLine(LogTag::Ptr logtag,
                                   const std::string &data,
                                   const std::string &colour_init,
                                   const std::string &colour_reset)
{
    if (log_meta && metadata && !metadata->empty())
    {
        dest << (timestamp ? GetTimestamp() : "") << " "
             << colour_init;

        if (!metadata->empty() && logtag)
        {
            dest << logtag->str(true) << " ";
        }
        dest << metadata << colour_reset
             << std::endl;
    }

    dest << (timestamp ? GetTimestamp() : "") << " "
         << colour_init;
    if (prepend_prefix && logtag)
    {
        dest << logtag->str(true) << " ";
    }
    dest << data << colour_reset << std::endl;

    if (metadata)
    {
        metadata->clear();
    }
}



//
//  ColourStreamWriter - implementation
//
ColourStreamWriter::ColourStreamWriter(std::ostream &dst, ColourEngine *ce)
    : StreamLogWriter(dst), colours(ce)
{
}


const std::string ColourStreamWriter::GetLogWriterInfo() const
{
    return std::string("ColourStreamWriter");
}


void ColourStreamWriter::WriteLogLine(LogTag::Ptr logtag,
                                      const LogGroup grp,
                                      const LogCategory ctg,
                                      const std::string &data,
                                      const std::string &colour_init,
                                      const std::string &colour_reset)
{
    switch (colours->GetColourMode())
    {
    case ColourEngine::ColourMode::BY_CATEGORY:
        LogWriter::WriteLogLine(logtag,
                                grp,
                                ctg,
                                data,
                                colours->ColourByCategory(ctg),
                                colours->Reset());
        return;

    case ColourEngine::ColourMode::BY_GROUP:
        {
            std::string grpcol = colours->ColourByGroup(grp);
            // Highlights parts of the log event which are higher
            // than LogCategory::INFO
            std::string ctgcol = (LogCategory::INFO < ctg
                                      ? colours->ColourByCategory(ctg)
                                      : grpcol);
            LogWriter::WriteLogLine(logtag,
                                    grp,
                                    ctg,
                                    grpcol + data,
                                    ctgcol,
                                    colours->Reset());
        }
        break;

    default:
        LogWriter::Write(grp, ctg, data);
        return;
    }
}
