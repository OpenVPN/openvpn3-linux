//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017-2018 OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017-2018 David Sommerseth <davids@openvpn.net>
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

#pragma once

/**
 * @file   logger.hpp
 *
 * @brief  Main log handler class, handles all the Log signals being sent
 */


class Logger : public LogConsumer,
               public RC<thread_unsafe_refcount>
{
public:
    typedef RCPtr<Logger> Ptr;

    enum class LogColour : std::uint_fast8_t {
        BLACK,   BRIGHT_BLACK,
        RED,     BRIGHT_RED,
        GREEN,   BRIGHT_GREEN,
        YELLOW,  BRIGHT_YELLOW,
        BLUE,    BRIGHT_BLUE,
        MAGENTA, BRIGHT_MAGENTA,
        CYAN,    BRIGHT_CYAN,
        WHITE,   BRIGHT_WHITE
    };


    Logger(GDBusConnection *dbuscon, std::string tag,
           std::string busname, std::string interf,
           unsigned int log_level = 3, bool timestamp = false)
        : LogConsumer(dbuscon, interf, "", busname),
          log_tag(tag),
          timestamp(timestamp),
          colourscheme(""),
          timestampcolour(""),
          colourreset("")
    {
        SetLogLevel(log_level);
    }

    void SetColourScheme(LogColour foreground, LogColour background)
    {
        const char * fgcode;
        const char * bgcode;

        switch (foreground)
        {
        case LogColour::BLACK:
                fgcode = "30m";
                break;
        case LogColour::RED:
                fgcode = "31m";
                break;
        case LogColour::GREEN:
                fgcode = "32m";
                break;
        case LogColour::YELLOW:
                fgcode = "33m";
                break;
        case LogColour::BLUE:
                fgcode = "34m";
                break;
        case LogColour::MAGENTA:
                fgcode = "35m";
                break;
        case LogColour::CYAN:
                fgcode = "36m";
                break;
        case LogColour::WHITE:
                fgcode = "37m";
                break;
        case LogColour::BRIGHT_BLACK:
                fgcode = "1;30m";
                break;
        case LogColour::BRIGHT_RED:
                fgcode = "1;31m";
                break;
        case LogColour::BRIGHT_GREEN:
                fgcode = "1;32m";
                break;
        case LogColour::BRIGHT_YELLOW:
                fgcode = "1;33m";
                break;
        case LogColour::BRIGHT_BLUE:
                fgcode = "1;34m";
                break;
        case LogColour::BRIGHT_MAGENTA:
                fgcode = "1;35m";
                break;
        case LogColour::BRIGHT_CYAN:
                fgcode = "1;36m";
                break;
        case LogColour::BRIGHT_WHITE:
                fgcode = "1;37m";
                break;
        default:
                fgcode = "0m";
                break;
        }

        switch (background)
        {
        case LogColour::BLACK:
        case LogColour::BRIGHT_BLACK:
                bgcode = "40m";
                break;
        case LogColour::RED:
        case LogColour::BRIGHT_RED:
                bgcode = "41m";
                break;
        case LogColour::GREEN:
        case LogColour::BRIGHT_GREEN:
                bgcode = "42m";
                break;
        case LogColour::YELLOW:
        case LogColour::BRIGHT_YELLOW:
                bgcode = "43m";
                break;
        case LogColour::BLUE:
        case LogColour::BRIGHT_BLUE:
                bgcode = "44m";
                break;
        case LogColour::MAGENTA:
        case LogColour::BRIGHT_MAGENTA:
                bgcode = "45m";
                break;
        case LogColour::CYAN:
        case LogColour::BRIGHT_CYAN:
                bgcode = "46m";
                break;
        case LogColour::WHITE:
        case LogColour::BRIGHT_WHITE:
                bgcode = "47m";
                break;
        default:
                bgcode = "0m";
                break;
        }

        colourreset = "\033[0m";
        timestampcolour = colourreset + "\033[37m\033[" + std::string(bgcode);
        colourscheme = colourreset + "\033[" + std::string(fgcode)
                        + "\033[" + std::string(bgcode);
    }


    void SetTimestampFlag(const bool tstamp)
    {
        timestamp = tstamp;
    }


    /**
     *  Adds a LogGroup to be excluded when consuming log events
     * @param exclgrp LogGroup to exclude
     */
    void AddExcludeFilter(LogGroup exclgrp)
    {
        exclude_loggroup.push_back(exclgrp);
    }


    void ConsumeLogEvent(const std::string sender,
                         const std::string interface,
                         const std::string object_path,
                         const LogEvent& logev)
    {
        // If file log is active, skip logging to console
        if (GetLogActive())
        {
            return;
        }

        for (const auto& e : exclude_loggroup)
        {
            if (e == logev.group)
            {
                return;  // Don't do anything if this LogGroup is filtered
            }
        }

        std::cout << timestampcolour << (timestamp ? GetTimestamp() : "")
                  << colourscheme
                  << log_tag << ":: sender=" << sender
                  << ", interface=" << interface
                  << ", path=" << object_path
                  << std::endl
                  << (timestamp ? "                    " : "       ")
                  << logev
                  << std::endl << colourreset;
    }

private:
    std::string log_tag;
    bool timestamp;
    std::string colourscheme;
    std::string timestampcolour;
    std::string colourreset;
    std::vector<LogGroup> exclude_loggroup;
};
