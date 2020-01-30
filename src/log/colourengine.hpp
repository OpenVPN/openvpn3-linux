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

#include <memory>

#include "log-helpers.hpp"


/**
 *  This is just the abstract API which needs to be implemented
 *  by various colour engine implementations
 */
class ColourEngine
{
public:
    typedef std::unique_ptr<ColourEngine> Ptr;

    /**
     *  Colouring approaches
     */
    enum class ColourMode : uint8_t {
        BY_GROUP,     //!< Colours chosen based on the LogGroup identifier
        BY_CATEGORY   //!< Colours chosen based on the LogCategory identifier
    };

    /**
     *  Supported colours
     */
    enum class Colour : std::uint8_t {
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


    /**
     *  Changes the colour mode.  This determins if the colour scheme
     *  to use should be based on the LogGroup (ColourMode::BY_GROUP) or by
     *  LogCategory (ColourMode::BY_CATEGORY).  The default is BY_CATEGORY.
     *
     * @param m  ColourMode to use
     *
     */
    void SetColourMode(ColourMode m)
    {
        mode = m;
    }


    /**
     *  Retrieves the current colouring mode
     *
     * @return Returns ColourMode of the current setting
     *
     */
    ColourMode GetColourMode()
    {
        return mode;
    }

    /**
     *  Provides the colours to be used for a specific LogGroup type
     *
     * @param grp  LogGroup of the colour scheme to retrieve
     *
     * @return  Returns a std::string to be used to set the colour
     *
     */
    virtual const std::string ColourByGroup(LogGroup grp) = 0;


    /**
     *  Provides the colours to be used for a specific LogCategory type
     *
     * @param ctg  LogCategory of the colour scheme to retrieve
     *
     * @return  Returns a std::string to be used to set the colour
     *
     */
    virtual const std::string ColourByCategory(LogCategory ctg) = 0;


    /**
     * Virtual deconstructor to allow cleanup in inheritated classes
     */
    virtual ~ColourEngine() = default;


private:
    ColourMode mode = ColourMode::BY_CATEGORY;

};
