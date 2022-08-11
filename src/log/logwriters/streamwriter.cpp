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

StreamLogWriter::StreamLogWriter(std::ostream& dst)
    : LogWriter(), dest(dst)
{
}


StreamLogWriter::~StreamLogWriter()
{
    dest.flush();
}


void StreamLogWriter::Write(const std::string& data,
                            const std::string& colour_init,
                            const std::string& colour_reset)
{
    if (log_meta && !metadata.empty())
    {
        dest << (timestamp ? GetTimestamp() : "") << " "
             << colour_init;
        if (prepend_meta)
        {
             dest << metadata.GetMetaValue(prepend_label);
        }
        dest << metadata << colour_reset
             << std::endl;
        prepend_meta = false;
    }
    dest << (timestamp ? GetTimestamp() : "") << " "
         << colour_init;
    if (!prepend_label.empty())
    {
        dest << metadata.GetMetaValue(prepend_label);
    }
    dest << data << colour_reset << std::endl;
    prepend_label.clear();
    metadata.clear();
}


//
//  ColourStreamWriter - implementation
//
ColourStreamWriter::ColourStreamWriter(std::ostream& dst, ColourEngine *ce)
    : StreamLogWriter(dst), colours(ce)
{
}


void ColourStreamWriter::Write(const LogGroup grp,
                               const LogCategory ctg,
                               const std::string& data)
{
    switch (colours->GetColourMode())
    {
    case ColourEngine::ColourMode::BY_CATEGORY:
        LogWriter::Write(grp, ctg, data,
                         colours->ColourByCategory(ctg),
                         colours->Reset());
        return;

    case ColourEngine::ColourMode::BY_GROUP:
        {
            std::string grpcol = colours->ColourByGroup(grp);
            // Highlights parts of the log event which are higher than LogCategory::INFO
            std::string ctgcol = (LogCategory::INFO < ctg ? colours->ColourByCategory(ctg) : grpcol);
            LogWriter::Write(grp, ctg, grpcol + data,
                             ctgcol,
                             colours->Reset());
        }
        break;

    default:
        LogWriter::Write(grp, ctg, data);
        return;
    }
}
