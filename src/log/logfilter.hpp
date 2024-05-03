//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//

#pragma once

#include <cstdint>
#include <vector>
#include <gdbuspp/object/path.hpp>

#include "events/log.hpp"


namespace Log {

/**
 *  Helper class to enable filtering log events.  This is used
 *  to decide which log events are going to be further processed based
 *  on the current log level setting.
 */
class EventFilter
{
  public:
    virtual ~EventFilter() = default;

    /**
     *  Sets the log level.  This filters which log messages will
     *  be processed or not.  Valid values are 0-6.
     *
     *  Log level 0 - Only FATAL and Critical messages are logged
     *  Log level 1 - includes log level 0 + Error messages
     *  Log level 2 - includes log level 1 + Warning messages
     *  Log level 3 - includes log level 2 + informational messages
     *  Log level 4 - includes log level 3 + Verb 1 messages
     *  Log level 5 - includes log level 4 + Verb 2 messages
     *  Log level 6 - includes log level 5 + Debug messages (everything)
     *
     * @param loglev  uint32_t with the log level to use
     */
    void SetLogLevel(const uint32_t loglev);

    /**
     * Retrieves the log level in use
     *
     * @return uint32_t with values between 0-6
     *
     */
    uint32_t GetLogLevel() const noexcept;

    /**
     *  Adds an allowed path to the path filtering.  This is used when
     *  proxying log and status events, to filter what is being forwarded.
     *
     * @param path  DBus::Object::Path containing the D-Bus object path to add
     */
    void AddPathFilter(const DBus::Object::Path &path) noexcept;


  protected:
    /**
     *  Prepares the log filter
     *
     * @param log_level uint32_t of the default log level
     */
    EventFilter(const uint32_t log_level_val) noexcept;

    /**
     * Checks if the LogCategory matches a log level where
     * logging should happen
     *
     * @param logev  LogEvent where to extract the log category from
     *               for the filter
     *
     * @return  Returns true if this LogEvent should be logged, based
     *          on the log category in the LogEvent object
     */
    bool Allow(const Events::Log &logev) const noexcept;

    /**
     *  Checks if the path is on the path list configured via @AddPathFilter.
     *
     * @param path  DBus::Object::Path of path to check
     * @return Returns true if the path is allowed. If no paths has been added,
     *         it will always return true.
     */
    bool AllowPath(const DBus::Object::Path &path) const noexcept;


  private:
    uint32_t log_level;                           ///< Current log level setting
    std::vector<DBus::Object::Path> filter_paths; ///< List of paths to be processed
};

} // namespace Log
