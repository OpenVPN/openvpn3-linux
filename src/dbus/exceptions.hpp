//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017 - 2022  OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017 - 2022  David Sommerseth <davids@openvpn.net>
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

#include <exception>
#include <sstream>

#include <glib.h>

#include "config.h"

/**
 *   DBusException is thrown whenever a D-Bus related error happens
 */
class DBusException : public std::exception
{
public:
    DBusException(const std::string classn, const std::string& err, const char *filen, const unsigned int linenum, const char *fn) noexcept
    : errorstr(err)
    {
        prepare_sourceref(classn, filen, linenum, fn, errorstr);
    }


    DBusException(const std::string classn, std::string&& err, const char *filen, const unsigned int linenum, const char *fn) noexcept
    : errorstr(std::move(err))
    {
        prepare_sourceref(classn, filen, linenum, fn, errorstr);
    }


    virtual ~DBusException() noexcept = default;


    virtual const char* what() const noexcept
    {
        return sourceref.c_str();
    }


    /**
     *  Using @what() may return a modified error message, containing
     *  more debug details.  This method will instead just return the
     *  unmodified error message from the the DBusException constructor.
     *
     * @return  Returns a const char * containing the unmodified
     *          error message.
     */
    virtual const char* GetRawError() const noexcept
    {
        return errorstr.c_str();
    }


    /**
     *  Wrapper for more easily returning a DBusException exception
     *  back to an on going D-Bus method call.  This will transport the
     *  error back to the D-Bus caller.
     *
     * @param invocation Pointer to a invocation object of the on-going
     *                   method call
     */
    virtual void SetDBusError(GDBusMethodInvocation *invocation,
                              std::string errdomain)
    {
        std::string qdom = (!errdomain.empty() ? errdomain : "net.openvpn.v3.error.undefined");
        GError *dbuserr = g_dbus_error_new_for_dbus_error(qdom.c_str(), errorstr.c_str());
        g_dbus_method_invocation_return_gerror(invocation, dbuserr);
        g_error_free(dbuserr);
    }


    /**
     *  Wrapper for more easily returning a DBusException back
     *  to an on going D-Bus set/get property call.  This will transport
     *  the error back to the D-Bus caller
     *
     * @param dbuserror  Pointer to a GError pointer where the error will
     *                   be saved
     * @param domain     Error Quark domain used to classify the
     *                   authentication failure
     * @param code       A GIO error code, used for further error
     *                   error classification.  Look up G_IO_ERROR_*
     *                   entries in glib-2.0/gio/gioenums.h for details.
     */
    virtual void SetDBusError(GError **dbuserror, GQuark domain, gint code) const
    {
        g_set_error (dbuserror, domain, code, "%s", errorstr.c_str());
    }


protected:
    std::string sourceref;
    const std::string errorstr;

private:
    void prepare_sourceref(const std::string classn,
                           const char *filen, const unsigned int linenum,
                           const char *fn, const std::string err) noexcept
    {
#ifdef DEBUG_EXCEPTIONS
        sourceref = "{" + std::string(filen) + ":" + std::to_string(linenum)
                  + ", " + classn + "::" + std::string(fn) + "()} " + err;
#else
        sourceref = err;
#endif

    }

};

#define THROW_DBUSEXCEPTION(classname, fault_data) throw DBusException(classname, fault_data, __FILE__, __LINE__, __FUNCTION__)

/**
 *  Specian exception classes used by set/get properties calls.
 *  exceptions will be translated into a D-Bus error which the
 *  calling D-Bus client will receive.
 */

class DBusPropertyException : public std::exception
{
public:
    DBusPropertyException(GQuark domain,
                          gint code,
                          const std::string& interf,
                          const std::string& objp,
                          const std::string& prop,
                          const std::string& err) noexcept
    : errordomain(domain),
        errorcode(code),
        errorstr(err),
        detailed("")
    {
        detailed = std::string("[interface=") + interf
                    + std::string(", path=") + objp
                    + std::string(", property=") + prop
                    + std::string("] ")
                    + errorstr;
    }


    DBusPropertyException(GQuark domain,
                          gint code,
                          const std::string&& interf,
                          const std::string&& objp,
                          const std::string&& prop,
                          const std::string&& err) noexcept
    : errordomain(domain),
        errorcode(code),
        errorstr(err),
        detailed("")
    {
        detailed = std::string("[interface=") + interf
                    + std::string(", path=") + objp
                    + std::string(", property=") + prop
                    + std::string("] ")
                    + errorstr;
    }


    virtual ~DBusPropertyException() noexcept = default;


    virtual const char* what() const noexcept
    {
        return detailed.c_str();
    }


    virtual const char* GetRawError() const noexcept
    {
        return errorstr.c_str();
    }


    void SetDBusError(GError **error)
    {
        g_set_error(error, errordomain, errorcode,
                    "%s", errorstr.c_str());
    }


private:
    GQuark errordomain;
    guint errorcode;
    std::string errorstr;
    std::string detailed;
};
