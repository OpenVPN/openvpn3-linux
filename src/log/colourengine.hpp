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
 * @file   colourengine.hpp
 *
 * @brief  Colour Engine API, used by ColourWriter to colour
 *         the log output
 */

#pragma once

/**
 *  This is just the abstract API which needs to be implemented
 *  by various colour engine implementations
 */
class ColourEngine
{
public:
    /**
     *  Supported colours
     */
    enum class Colour : std::uint_fast8_t {
        NONE,
        BLACK,   BRIGHT_BLACK,
        RED,     BRIGHT_RED,
        GREEN,   BRIGHT_GREEN,
        YELLOW,  BRIGHT_YELLOW,
        BLUE,    BRIGHT_BLUE,
        MAGENTA, BRIGHT_MAGENTA,
        CYAN,    BRIGHT_CYAN,
        WHITE,   BRIGHT_WHITE
    };

    /**
     *  Needs to be called to specify the colours to be used in the output
     *
     * @param fg  ColourEngine::Colour to use as foreground colour
     * @param bg  ColourEngine::Colour to use as backround colour
     *
     * @return  Returns a string containing to be used to colour the output
     */
    virtual const std::string Set(Colour fg, Colour bg) = 0;

    /**
     *  Removes any colour settings, returning back to default output mode
     *
     * @return  Returns the string needed to reset the colour to default
     */
    virtual const std::string Reset() = 0;
};
