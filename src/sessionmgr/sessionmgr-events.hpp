//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2020         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020         David Sommerseth <davids@openvpn.net>
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
 * @file   sessionmgrevents.hpp
 *
 * @brief  Definition of the SessionManager::Event class, which maps to the
 *         SessionManagerEvent signal the main net.openvpn.v3.sessions
 *         object can send.
 */


#pragma once

#include <string>
#include <sstream>

#include "dbus/core.hpp"
#include "dbus/exceptions.hpp"
#include "sessionmgr-exceptions.hpp"

namespace SessionManager
{
    enum class EventType : std::uint16_t
    {
        UNSET = 0,
        SESS_CREATED,
        SESS_DESTROYED
    };


    /**
     *  This class maps to the SessionManagerEvent D-Bus signal
     */
    class Event
    {
    public:
        /**
         *  Initialize a new SessionManager::Event
         *
         * @param path    D-Bus path of the session this event is related to
         * @param type    SessionManager::EventType of the event
         * @param owner   uid_t of the owner of the session this event is
         *                related to
         */
        Event(const std::string& path, EventType type, uid_t owner);

        /**
         *  Initialize a new SessionManager::Event from a D-Bus GVariant
         *  object
         *
         * @param params  GVariant pointer to the signal event to parse
         */
        Event(GVariant *params);

        /**
         *   Initialize an empty SessionManager::Event object
         */
        Event() noexcept {}
        ~Event() = default;


        /**
         *  Check if the SessionManager::Event object is empty/not-set
         *
         * @return  Returns true if none of the event members has been set
         */
        bool empty() const noexcept;


        /**
         *  Retrieve a prepeared Glib2 GVariant object containing a
         *  prepared D-Bus signal response based on the content of this
         *  object
         *
         * @return  Returns a pointer to a new GVariant object
         */
        GVariant * GetGVariant() const noexcept;


        /**
         *  Get a human readable representation of a SessionManager::EventType
         *
         * @param type       SessionManager::EventType to parse
         * @param tech_form  If the resulting string should be technical
         *                   formatted or more user descriptive.  Defaults
         *                   to false.
         *
         * @return  Returns a std::string of the representation of EventType
         */
        static const std::string TypeStr(const EventType type,
                                         bool tech_form = false) noexcept
        {
            switch (type)
            {
            case EventType::UNSET:
                return "[UNSET]";

            case EventType::SESS_CREATED:
                return (tech_form ? "SESS_CREATED" : "Session created");

            case EventType::SESS_DESTROYED:
                return (tech_form ? "SESS_DESTROYED" : "Session destroyed");
            }
            return "[UNKNOWN]";
        }


        /**
         *  Get the proper D-Bus signal introspection data
         *
         * @return  std::string containing the <signal/>  introspection
         *          section for the D-Bus SessionManagerEvent signal
         */
        static const std::string GetIntrospection() noexcept
        {
            return "         <signal name='SessionManagerEvent'>"
                   "           <arg type='o' name='path'/>"
                   "           <arg type='q' name='type'/>"
                   "           <arg type='u' name='owner'/>"
                   "         </signal>";
        }

        bool operator==(const Event& compare) const;
        bool operator!=(const Event& compare) const;

        friend std::ostream& operator<<(std::ostream& os,
                                        const Event& e)
        {
            return os << TypeStr(e.type)
                      << "; owner: " << std::to_string(e.owner)
                      << ", path: " << e.path;
        }

        std::string path = "";
        EventType type = EventType::UNSET;
        uid_t owner = 65535;
    };

}  // namespace SessionManager
