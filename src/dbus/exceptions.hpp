//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
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

#ifndef OPENVPN3_DBUS_EXCEPTIONS_HPP
#define OPENVPN3_DBUS_EXCEPTIONS_HPP

#include <exception>
#include <sstream>

namespace openvpn
{
    /**
     *   DBusException is thrown whenever a D-Bus related error happens
     */
    class DBusException : public std::exception
    {
    public:
        DBusException(const std::string classn, const std::string& err, const char *filen, const unsigned int linenum, const char *fn) noexcept
        : classname(classn), errorstr(err), filename(std::string(filen)), line(linenum), method(std::string(fn))
        {
        }


        DBusException(const std::string classn, std::string&& err, const char *filen, const unsigned int linenum, const char *fn) noexcept
        : classname(classn), errorstr(std::move(err)) , filename(std::string(filen)), line(linenum), method(std::string(fn))
        {
        }


        virtual ~DBusException() throw() {}


        virtual const char* what() const throw()
        {
            std::stringstream ret;
            ret << "[DBusException: "
                << filename << ":" << line << ", "
                << classname << "::" << method << "()] " << errorstr;
            return ret.str().c_str();
        }


        const std::string& err() const noexcept
        {
            std::stringstream ret;
            ret << "[" << filename << ":" << line << ", "
                << classname << "::" << method << "()] "
                << errorstr;
            return std::move(ret.str());
        }


        std::string getRawError() const noexcept
        {
            return errorstr;
        }


    protected:
        const std::string classname;
        const std::string errorstr;
        const std::string filename;
        const unsigned int line;
        const std::string method;

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
            interface(interf),
            object_path(objp),
            property(prop),
            errorstr(err)
        {
        }


        DBusPropertyException(GQuark domain,
                              gint code,
                              const std::string&& interf,
                              const std::string&& objp,
                              const std::string&& prop,
                              const std::string&& err) noexcept
        : errordomain(domain),
            errorcode(code),
            interface(interf),
            object_path(objp),
            property(prop),
            errorstr(err)
        {
        }


        virtual ~DBusPropertyException() throw() {}


        virtual const char* what() const throw()
        {
            std::stringstream ret;
            ret << "[DBusPropertyException: "
                << "interface=" << interface
                << ", path=" << object_path
                << ", property=" << property
                << "] " << errorstr;
            return ret.str().c_str();
        }


        const std::string& err() const noexcept
        {
            std::stringstream ret;
            ret << "["
                << "interface=" << interface
                << ", path=" << object_path
                << ", property=" << property
                << "] " << errorstr;
            return std::move(ret.str());
        }


        std::string getRawError() const noexcept
        {
            return errorstr;
        }


        void SetDBusError(GError **error)
        {
            g_set_error(error, errordomain, errorcode,
                        "%s", errorstr.c_str());
        }


    private:
        GQuark errordomain;
        guint errorcode;
        std::string interface;
        std::string object_path;
        std::string property;
        std::string errorstr;
    };
};
#endif // OPENVPN3_DBUS_EXCEPTIONS_HPP
