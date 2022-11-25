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
 * @file   journald.hpp
 *
 * @brief  Declaration of the JournaldWriter implementation of LogWriter
 */

#pragma once

#include "config.h"
#include "log/logwriter.hpp"


#ifdef HAVE_SYSTEMD
/**
 *  LogWriter implementation, writing to systemd journal
 */
class JournaldWriter : public LogWriter
{
  public:
    JournaldWriter();
    virtual ~JournaldWriter() = default;

    const std::string GetLogWriterInfo() const override;

    /**
     *  We presume journald will always add timestamps to its logging,
     *  so we will return true regardless of what an external user wants.
     *
     *  In addition, JorunaldWriter doesn't even care about the timestamp
     *  flag.  So try to present a value which is more likely true regardless
     *  of this internal flag.
     *
     * @return Will always return true.
     */
    bool TimestampEnabled() override;

    void Write(const std::string &data,
               const std::string &colour_init = "",
               const std::string &colour_reset = "") override;

    void Write(const LogGroup grp,
               const LogCategory ctg,
               const std::string &data,
               const std::string &colour_init,
               const std::string &colour_reset) override;

    void Write(const LogEvent &event) override;
};
#endif // HAVE_SYSTEMD
