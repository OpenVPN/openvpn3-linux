//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   journald.hpp
 *
 * @brief  Declaration of the JournaldWriter implementation of LogWriter
 */

#pragma once

#include "log/logwriter.hpp"


#ifdef HAVE_SYSTEMD
/**
 *  LogWriter implementation, writing to systemd journal
 */
class JournaldWriter : public LogWriter
{
  public:
    JournaldWriter(const std::string &log_sender);
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

    void Write(const Events::Log &event) override;


  protected:
    void WriteLogLine(LogTag::Ptr logtag,
                     const std::string &data,
                     const std::string &colour_init = "",
                     const std::string &colour_reset = "") override;

    void WriteLogLine(LogTag::Ptr logtag,
                     const LogGroup grp,
                     const LogCategory ctg,
                     const std::string &data,
                     const std::string &colour_init,
                     const std::string &colour_reset) override;

  private:
    const std::string log_sender;
};
#endif // HAVE_SYSTEMD
