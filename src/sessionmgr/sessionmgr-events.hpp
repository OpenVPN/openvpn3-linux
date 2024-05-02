//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   sessionmgr-events.hpp
 *
 * @brief  Definition of the SessionManager::Event class, which maps to the
 *         SessionManagerEvent signal the main net.openvpn.v3.sessions
 *         object can send.
 */

#pragma once

#include <cstdint>
#include <string>
#include <sstream>
#include <gdbuspp/glib2/utils.hpp>
#include <gdbuspp/signals/group.hpp>


namespace SessionManager {

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
    Event(const std::string &path, EventType type, uid_t owner);

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
    Event() noexcept
    {
    }
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
    GVariant *GetGVariant() const noexcept;


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
                                     bool tech_form = false) noexcept;

    /**
     *  Get the proper D-Bus signal introspection data
     *
     * @return  DBus::Signals::SignalArgList describing the signal type
     */
    static const DBus::Signals::SignalArgList SignalDeclaration() noexcept;

    bool operator==(const Event &compare) const;
    bool operator!=(const Event &compare) const;

    friend std::ostream &operator<<(std::ostream &os,
                                    const Event &e)
    {
        return os << TypeStr(e.type)
                  << "; owner: " << std::to_string(e.owner)
                  << ", path: " << e.path;
    }


    std::string path = "";
    EventType type = EventType::UNSET;
    uid_t owner = 65535;
};


} // namespace SessionManager

template <>
inline const char *glib2::DataType::DBus<SessionManager::EventType>() noexcept
{
    return glib2::DataType::DBus<std::uint16_t>();
}

template <>
inline SessionManager::EventType glib2::Value::Get<SessionManager::EventType>(GVariant *v) noexcept
{
    return static_cast<SessionManager::EventType>(glib2::Value::Get<uint16_t>(v));
}

template <>
inline SessionManager::EventType glib2::Value::Extract<SessionManager::EventType>(GVariant *v, int elm) noexcept
{
    return static_cast<SessionManager::EventType>(glib2::Value::Extract<uint16_t>(v, elm));
}

inline std::ostream &operator<<(std::ostream &os, const SessionManager::EventType &obj)
{
    return os << static_cast<uint16_t>(obj);
}
