//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, version 3 of the License
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef OPENVPN3_HELPERS_LOG_HPP
#define OPENVPN3_HELPERS_LOG_HPP

#include <cstdint>
#include <sstream>
#include <array>

class LogException : public std::exception
    {
    public:
        LogException(const std::string& err, const char *filen, const unsigned int linenum, const char *fn) noexcept
        : errorstr(err), filename(std::string(filen)), line(linenum), method(std::string(fn))
        {
        }

        LogException(std::string&& err, const char *filen, const unsigned int linenum, const char *fn) noexcept
        : errorstr(std::move(err)) , filename(std::string(filen)), line(linenum), method(std::string(fn))
        {
        }

        virtual ~LogException() throw() {}

        virtual const char* what() const throw()
        {
            std::stringstream ret;
            ret << "[LogException: "
                << filename << ":" << line << ", "
                << method << "()] " << errorstr;
            return ret.str().c_str();
        }
        const std::string& err() const noexcept
        {
            std::stringstream ret;
            ret << "[" << filename << ":" << line << ", "
                << method << "()] "
                << errorstr;
            return std::move(ret.str());
        }

    private:
        std::string errorstr;
        std::string filename;
        const unsigned int line;
        std::string method;

    };

#define THROW_LOGEXCEPTION(fault_data) throw LogException(fault_data, __FILE__, __LINE__, __FUNCTION__)


/**
 * Log groups is used to classify the source of log events
 */
const uint8_t LogGroupCount = 8;
enum class LogGroup : std::uint_fast8_t {
        UNDEFINED,              /**< Default - should not be used in code, but is here to detect errors */
        MASTERPROC,             /**< Master process (main openvpn-manager) */
        CONFIGMGR,              /**< Configuration Manager process (child of openvpn-manager)*/
        SESSIONMGR,             /**< Session manager process (child of openvpn-manager) */
        BACKENDSTART,           /**< Backend starter process (openvpn3-service-backendstart) */
        LOGGER,                 /**< Logger process (child of openvpn-manager) */
        BACKENDPROC,            /**< Session process (openvpn-service-client) */
        CLIENT                  /**< OpenVPN 3 Core tunnel object in the session process */
};

const std::array<const std::string, LogGroupCount> LogGroup_str = {
        "[[UNDEFINED]]",
        "Master Process",
        "Config Manager",
        "Session Manager",
        "Backend Starter",
        "Logger",
        "Backend Session Process",
        "Client"
};

enum class LogCategory : uint_fast8_t {
        DEBUG,                  /**< Debug messages */
        VERB2,                  /**< Even more details */
        VERB1,                  /**< More details */
        INFO,                   /**< Informational messages */
        WARN,                   /**< Warnings - important issues which might need attention*/
        ERROR,                  /**< Errors - These must be fixed for successful operation */
        CRIT,                   /**< Critical - These requires users attention */
        FATAL                   /**< Fatal errors - The current operation is going to stop */
};

const std::array<const std::string, 8> LogCategory_str = {
        "DEBUG",                // LogFlags::DEBUG
        "VERB2",                // LogFlags::VERB2
        "VERB1",                // LogFlags::VERB1
        "INFO",                 // LogFlags::INFO
        "WARNING",              // LogFlags::WARN
        "-- ERROR --",          // LogFlags::ERROR
        "!! CRITICAL !!",       // LogFlags::CRIT
        "**!! FATAL !!**",      // LogFlags::FATAL
};

const std::string LogPrefix(LogGroup group, LogCategory catg)
{
        if ((uint_fast8_t) group >= LogGroupCount) {
            THROW_LOGEXCEPTION("Invalid Log Group value");
        }

        if ((uint8_t) catg < 0 || (uint8_t) catg > 7) {
            THROW_LOGEXCEPTION("Invalid category in log flags");
        }

        std::stringstream ret;
        ret << LogGroup_str[(uint8_t) group] << " "
            << LogCategory_str[(uint8_t)catg] << ": ";
        return ret.str();
}

#endif // OPENVPN3_LOG_HELPERS_HPP

