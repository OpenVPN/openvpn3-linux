//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

#pragma once

/**
 * @file   helpers.hpp
 *
 * @brief  Misc helper functions and related types for
 *         the openvpn3 session commands
 */

#include <cstdint>

#include "common/cmdargparser-exceptions.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"


namespace ovpn3cli::session {

class SessionException : public CommandArgBaseException
{
  public:
    SessionException(const std::string &msg);
};



/**
 *  Defines which start modes used by @start_session()
 */
enum class SessionStartMode : std::uint8_t
{
    NONE,   /**< Undefined */
    START,  /**< Call the Connect() method */
    RESUME, /**< Call the Resume() method */
    RESTART /**< Call the Restart() method */
};



/**
 *  Start, resume or restart a VPN session
 *
 *  This code is shared among all the start methods to ensure the
 *  user gets a chance to provide user input if the remote server
 *  wants that for re-authentication.
 *
 * @param session       OpenVPN3SessionProxy to the session object to
 *                      operate on
 * @param initial_mode  SessionStartMode defining how this tunnel is
 *                      to be (re)started
 * @param timeout       Connection timeout. If exceeding this, the connection
 *                      attempt is aborted.
 * @param background    Connection should be started in the background after
 *                      basic credentials has been provided.  Only considered
 *                      for initial_mode == SessionStartMode::START.
 *
 * @throws SessionException if any issues related to the session itself.
 */
void start_session(SessionManager::Proxy::Session::Ptr session,
                   SessionStartMode initial_mode,
                   int timeout,
                   bool background = false);

/**
 *  Start
 *
 * @param url
 * @return true
 * @return false
 */
bool start_url_auth(const std::string &url);


/**
 *  Parse ConnectionStats container into a human readable string
 *
 * @param stats
 * @return std::string
 */
std::string statistics_plain(ConnectionStats &stats);

} // namespace ovpn3cli::session


// Implemented in config-import.cpp
std::string import_config(const std::string &filename,
                          const std::string &cfgname,
                          const bool single_use,
                          const bool persistent,
                          const std::vector<std::string> &tags);
