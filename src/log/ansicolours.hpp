//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         David Sommerseth <davids@openvpn.net>
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
 * @file   ansicolours.hpp
 *
 * @brief  ColourEngine ANSI Terminal colour code implementation
 */

#pragma once

#include "colourengine.hpp"

/**
 *  Class implementing the ColourEngine to be used to make
 *  output to terminals colourful
 */
class ANSIColours : public ColourEngine
{
public:
    ANSIColours() = default;
    ~ANSIColours() override = default;

    const std::string Set(Colour foreground, Colour background) override
    {
        const char * fgcode;
        const char * bgcode;

        switch (foreground)
        {
        case Colour::BLACK:
                fgcode = "30m";
                break;
        case Colour::RED:
                fgcode = "31m";
                break;
        case Colour::GREEN:
                fgcode = "32m";
                break;
        case Colour::YELLOW:
                fgcode = "33m";
                break;
        case Colour::BLUE:
                fgcode = "34m";
                break;
        case Colour::MAGENTA:
                fgcode = "35m";
                break;
        case Colour::CYAN:
                fgcode = "36m";
                break;
        case Colour::WHITE:
                fgcode = "37m";
                break;
        case Colour::BRIGHT_BLACK:
                fgcode = "1;30m";
                break;
        case Colour::BRIGHT_RED:
                fgcode = "1;31m";
                break;
        case Colour::BRIGHT_GREEN:
                fgcode = "1;32m";
                break;
        case Colour::BRIGHT_YELLOW:
                fgcode = "1;33m";
                break;
        case Colour::BRIGHT_BLUE:
                fgcode = "1;34m";
                break;
        case Colour::BRIGHT_MAGENTA:
                fgcode = "1;35m";
                break;
        case Colour::BRIGHT_CYAN:
                fgcode = "1;36m";
                break;
        case Colour::BRIGHT_WHITE:
                fgcode = "1;37m";
                break;
        default:
                fgcode = "0m";
                break;
        }

        switch (background)
        {
        case Colour::BLACK:
        case Colour::BRIGHT_BLACK:
                bgcode = "\033[40m";
                break;
        case Colour::RED:
        case Colour::BRIGHT_RED:
                bgcode = "\033[41m";
                break;
        case Colour::GREEN:
        case Colour::BRIGHT_GREEN:
                bgcode = "\033[42m";
                break;
        case Colour::YELLOW:
        case Colour::BRIGHT_YELLOW:
                bgcode = "\033[43m";
                break;
        case Colour::BLUE:
        case Colour::BRIGHT_BLUE:
                bgcode = "\033[44m";
                break;
        case Colour::MAGENTA:
        case Colour::BRIGHT_MAGENTA:
                bgcode = "\033[45m";
                break;
        case Colour::CYAN:
        case Colour::BRIGHT_CYAN:
                bgcode = "\033[46m";
                break;
        case Colour::WHITE:
        case Colour::BRIGHT_WHITE:
                bgcode = "\033[47m";
                break;

        case Colour::NONE:
        default:
                bgcode = "";
                break;
        }

        return Reset() + "\033[" + std::string(fgcode)
                       + "" + std::string(bgcode);

    }


    const std::string Reset() override
    {
        return "\033[0m";
    }



    const std::string ColourByGroup(LogGroup grp) override
    {
        switch(grp)
        {
        case LogGroup::CONFIGMGR:
            return Set(ColourEngine::Colour::BRIGHT_WHITE,
                                ColourEngine::Colour::GREEN);

        case LogGroup::SESSIONMGR:
            return Set(ColourEngine::Colour::BRIGHT_WHITE,
                                ColourEngine::Colour::BLUE);

        case LogGroup::BACKENDSTART:
            return Set(ColourEngine::Colour::BRIGHT_WHITE,
                                ColourEngine::Colour::CYAN);

        case LogGroup::LOGGER:
            return Set(ColourEngine::Colour::BRIGHT_GREEN,
                                ColourEngine::Colour::NONE);

        case LogGroup::BACKENDPROC:
            return Set(ColourEngine::Colour::CYAN,
                                ColourEngine::Colour::NONE);

        case LogGroup::CLIENT:
            return Set(ColourEngine::Colour::BRIGHT_YELLOW,
                                ColourEngine::Colour::NONE);

        case LogGroup::UNDEFINED:
        case LogGroup::MASTERPROC:
        default:
            return "";
        }
    }


    const std::string ColourByCategory(LogCategory ctg) override
    {
        switch(ctg)
        {
        case LogCategory::DEBUG:
            return Set(ColourEngine::Colour::BRIGHT_BLUE,
                       ColourEngine::Colour::NONE);

        case LogCategory::VERB2:
            return Set(ColourEngine::Colour::BRIGHT_CYAN,
                       ColourEngine::Colour::NONE);

        case LogCategory::VERB1:
            return "";

        case LogCategory::INFO:
            return Set(ColourEngine::Colour::BRIGHT_WHITE,
                       ColourEngine::Colour::NONE);

        case LogCategory::WARN:
            return Set(ColourEngine::Colour::BRIGHT_YELLOW,
                       ColourEngine::Colour::NONE);

        case LogCategory::ERROR:
            return Set(ColourEngine::Colour::BRIGHT_RED,
                       ColourEngine::Colour::NONE);

        case LogCategory::CRIT:
            return Set(ColourEngine::Colour::BRIGHT_WHITE,
                       ColourEngine::Colour::RED);

        case LogCategory::FATAL:
            return Set(ColourEngine::Colour::BRIGHT_YELLOW,
                       ColourEngine::Colour::RED);

        default:
            return "";
        }
    }
};

