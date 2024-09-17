//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   resolver-settings.hpp
 *
 *
 * @brief  Declares the class which is responsible for managing DNS
 *         resolver settings
 */
#pragma once

#include "build-config.h"

#include <memory>
#include <sstream>
#include <vector>

#include "log/core-dbus-logger.hpp"
#include <openvpn/client/dns.hpp>


namespace NetCfg {
namespace DNS {


enum class Scope
{
    GLOBAL, ///<  Can resolve all DNS queries
    TUNNEL, ///<  Will only resolve DNS queries for domains on this tunnel
};


class ResolverSettings
{
  public:
    typedef std::shared_ptr<ResolverSettings> Ptr;

    /**
     *  Construct a new ResolverSettings object for a specific
     *  network interface.  The index is used to link which
     *  VPN session this ResolverSettings object is related to
     *
     * @param index   Numeric VPN session index link
     *
     * @return Initialized ResolverSettings::Ptr object
     */
    [[nodiscard]] static Ptr Create(const ssize_t if_index = 0)
    {
        return Ptr(new ResolverSettings(if_index));
    }

    ~ResolverSettings();

    ResolverSettings(const ResolverSettings::Ptr &orig);


    /**
     *  Retrieves the sorting index of this particular resolver settings
     *  object.  The sorting index is used to preserve a specific order
     *  of how resolver settings are applied on the system.
     *
     * @return  Returns an ssize_t value of the current index
     */
    const ssize_t GetIndex() const noexcept;


    /**
     *  Set the activation flag for this ResolverSettings object
     *  to true.
     *
     *  The resolver-backend will use this flag to understand
     *  if these settings should be applied or not to the system.
     */
    void Enable() noexcept;


    /**
     *  Set the activation flag for this ResolverSettings object
     *  to false.
     *
     *  The resolver-backend will use this flag to understand
     *  if these settings should be applied or not to the system.
     */
    void Disable() noexcept;


    /**
     *  Retrieve the activation status of this settings object
     *
     *  The resolver-backend will use this flag to understand
     *  if these settings should be applied or not to the system.
     *
     * @return  Returns true if this ResolverSettings object has its
     *          changes enabled.  Otherwise false is returned.
     */
    bool GetEnabled() const noexcept;


    /**
     *  Prepare this ResolverSettings object for removal and deletion.
     *  This object will be deleted when SettingsManager::ApplySettings()
     *  is being executed.
     *
     *  This delay is needed to allow the SettingsManager to issue
     *  NetworkChange notification events of removed DNS resolver
     *  settings.
     */
    void PrepareRemoval() noexcept;


    /**
     *  Retrieve the "PrepareRemoval" state.  If @PrepareRemoval()
     *  has been called, this will return true.
     *
     * @return  Returns true if this settings object is slated for
     *          removal.
     */
    bool GetRemovable() const noexcept;


    /**
     *  Check if there are resolver changes available.  If neither
     *  DNS resolver servers or search domains has been set, it will
     *  return false.
     *
     * @return  Returns true if there are changes present, otherwise false
     */
    bool ChangesAvailable() const noexcept;


    /**
     *  Set the device name the settings in this object is attached to
     *
     *  @param devname  std::string with the human readable device name
     */
    void SetDeviceName(const std::string &devname) noexcept;


    /**
     *  Retrieve the tun device name the settings in this object
     *  is attached to
     *
     * @return Returns std::string of the device name
     */
    std::string GetDeviceName() const noexcept;


    /**
     *  Sets the DNS resolver scope
     * @param scope  DNS::Scope to use
     */
    void SetDNSScope(const DNS::Scope scope) noexcept;


    /**
     *  Retrieve the current DNS resolver scope
     *
     * @return  Returns DNS::Scope of the current resolver scope
     */
    const DNS::Scope GetDNSScope() const noexcept;


    /**
     *  Retrieve the current DNS resolver scope as a string
     *
     * @return Returns a std::string representation of the current DNS::Scope
     */
    const char *GetDNSScopeStr() const noexcept;


    /**
     *  Adds a new single DNS name server
     *
     * @param server  std::string of DNS server to enlist
     */
    void AddNameServer(const std::string &server);


    /**
     *  Clear the list of DNS name servers
     */
    void ClearNameServers();


    /**
     *  Retrieve the current list of DNS name servers
     *
     * @param  removable   Boolean flag to enable retrieval of
     *                     name servers even if this object is flagged
     *                     as removable.  Default is false, which will
     *                     return an empty list if this object is queued
     *                     for deletion.
     *
     * @return  Returns a std::vector<std::string> of registered
     *          DNS name servers
     */
    std::vector<std::string> GetNameServers(bool removable = false) const noexcept;


    /**
     *  Adds a new single DNS search domain
     *
     * @param server  std::string of DNS search domain to add
     */
    void AddSearchDomain(const std::string &domain);


    /**
     *  Clear the list of DNS search domains
     */
    void ClearSearchDomains();

    /**
     *  Retrieve the current list of DNS search domains
     *
     * @param  removable   Boolean flag to enable retrieval o
     *                     search domains even if this object is flagged
     *                     as removable.  Default is false, which will
     *                     return an empty list if this object is queued
     *                     for deletion.
     *
     * @return  Returns a std::vector<std::string> of all registered
     *          DNS search domains
     */
    std::vector<std::string> GetSearchDomains(bool removable = false) const noexcept;

    /**
     *  Set the DNSSEC mode for the interface
     *
     *  Supported modes:
     *
     *   - DnsServer::Security::No        -  DNSSEC is not enabled
     *   - DnsServer::Security::Yes       -  DNSSEC is enabled and mandatory
     *   - DnsServer::Security::Optional  -  Opportunistic DNSSEC enabled
     *   - DnsServer::Security::Unset     -  DNSSEC is not configured, system
     *                                       default settings will be used.
     *
     *  They are defined in openvpn3-core/openvpn/client/dns.hpp
     *  in the openvpn namespace
     *
     * @param mode  openvpn::DnsServer::Security
     */
    void SetDNSSEC(const openvpn::DnsServer::Security &mode);

    /**
     *  Retrieve the DNSSEC mode for the interface
     *
     *  Supported return values:
     *
     *   - DnsServer::Security::No        -  DNSSEC is not enabled
     *   - DnsServer::Security::Yes       -  DNSSEC is enabled and mandatory
     *   - DnsServer::Security::Optional  -  Opportunistic DNSSEC enabled
     *   - DnsServer::Security::Unset     -  DNSSEC is not configured, system
     *                                       default settings will be used.
     *
     *  They are defined in openvpn3-core/openvpn/client/dns.hpp
     *  in the openvpn namespace
     *
     * @return openvpn::DnsServer::Security
     */
    openvpn::DnsServer::Security GetDNSSEC() const;

    /**
     *  Retrieve the DNSSEC mode for the interfacse as std::string
     *
     *  Expected return values should be one of these:
     *
     *   - no        -  DNSSEC is not enabled
     *   - yes       -  DNSSEC is enabled and mandatory
     *   - optional  -  Opportunistic DNSSEC enabled
     *   - unset     -  DNSSEC is not configured, system default settings
     *                  will be used.
     *
     * @return std::string
     */
    std::string GetDNSSEC_string() const;

    /**
     *  Set the transport method used to connect to the DNS server
     *
     *  Supported modes are:
     *
     *  - DnsServer::Transport::Plain  -  Unencrypted (traditional port 53)
     *  - DnsServer::Transport::TLS    -  DNS over TLS (encrypted)
     *  - DnsServer::Transport::HTTPS  -  DNS over HTTPS
     *  - DnsServer::Transport::Unset  -  Not set, use the default of the
     *                                    backend resolver
     *
     *  NOTE:  Not all resolver backends will support this setting or all
     *         of the alternatives.
     *
     * @param mode    openvpn::DnsServer::Transport
     */
    void SetDNSTransport(const openvpn::DnsServer::Transport &mode);

    /**
     *  Get the transport method used to connect to the DNS server
     *
     *  Supported values are:
     *
     *  - DnsServer::Transport::Plain  -  Unencrypted (traditional port 53)
     *  - DnsServer::Transport::TLS    -  DNS over TLS (encrypted)
     *  - DnsServer::Transport::HTTPS  -  DNS over HTTPS
     *  - DnsServer::Transport::Unset  -  Not set, use the default of the
     *                                    backend resolver
     *
     * @return openvpn::DnsServer::Transport
     */
    openvpn::DnsServer::Transport GetDNSTransport() const;

    /**
     *  Get the transport method used to connect to the DNS server,
     *  returned as a std::string
     *
     *  Supported values are:
     *
     *  - "plain"  -  Unencrypted (traditional port 53)
     *  - "dot"    -  DNS over TLS (encrypted)
     *  - "doh"    -  DNS over HTTPS
     *  - "unset"  -  Not set, use the default of the backend resolver
     *
     * @return std::string
     */
    std::string GetDNSTransport_string() const;

    /**
     *  Makes it possible to write ResolverSettings in a readable format
     *  via iostreams, such as 'std::cout << rs', where rs is a
     *  ResolverSettings object.
     *
     * @param os  std::ostream where to write the data
     * @param rs  ResolverSettings object to write to the stream
     *
     * @return  Returns the provided std::ostream together with the
     *          decoded ResolverSertings information
     */
    friend std::ostream &operator<<(std::ostream &os,
                                    const ResolverSettings::Ptr rs)
    {
        if ((rs->name_servers.empty() && rs->search_domains.empty())
            || rs->prepare_removal)
        {
            return os << "(No DNS resolver settings)";
        }

        if (!rs->enabled)
        {
            return os << "(Settings not enabled)";
        }
        std::stringstream s;
        if (rs->name_servers.size() > 0)
        {
            s << "DNS resolvers: ";
            bool first = true;
            for (const auto &r : rs->name_servers)
            {
                s << (!first ? ", " : "") << r;
                first = false;
            }
        }
        if (rs->search_domains.size() > 0)
        {
            if (s.str().size() > 0)
            {
                s << " - ";
            }
            s << "Search domains: ";
            bool first = true;
            for (const auto &sd : rs->search_domains)
            {
                s << (!first ? ", " : "") << sd;
                first = false;
            }
        }
        return os << s.str();
    }


#ifdef __GIO_TYPES_H__ // Only add GLib/GDBus methods if this is already used
    /**
     *  Sets the DNS resolver scope based on a value from a set_property
     *  D-Bus call
     *
     *  @param params  GVariant object containing an (s) based string
     *                 of a textual representation of the scope
     *  @returns Returns a std::string of the received and accepted
     *           DNS resolver scope
     */
    const std::string SetDNSScope(GVariant *params);

    /**
     *  Adds DNS name servers based on an array of strings provided via
     *  a GVariant container of the (as) type.
     *
     * @param params  GVariant object containing an (as) based string
     *                array of elements to process
     *
     * @returns Returns a std::string list of added DNS servers,
     *          comma separated
     */
    const std::string AddNameServers(GVariant *params);

    /**
     *  Adds new DNS search domains based on an array of strings provided
     *  via a GVariant container of the (as) type.
     *
     * @param params  GVariant object containing an (as) based string
     *                array of elements to process
     */
    void AddSearchDomains(GVariant *params);

    /**
     *  Set the DNSSEC mode for the interface provided as a string via
     *  a GVariant container of the (s) type.
     *
     *  Supported values:
     *
     *   - "no"        -  DNSSEC is not enabled
     *   - "yes"       -  DNSSEC is enabled and mandatory
     *   - "optional"  -  Opportunistic DNSSEC enabled
     *
     * @param params  GVariant object containing a (s) based string containing
     *                the DNSSEC mode
     *
     * @return openvpn::DnsServer::Security of the parsed and set DNSSEC mode
     */
    openvpn::DnsServer::Security SetDNSSEC(GVariant *params);

    /**
     *  Set the transport method used to connect to the DNS server as a
     *  string via a GVariant container of the (s) type.
     *
     *  Supported values:
     *
     *  - "plain"  -  Unencrypted (traditional port 53)
     *  - "dot"    -  DNS over TLS (encrypted)
     *  - "doh"    -  DNS over HTTPS
     *
     * @param params
     * @return openvpn::DnsServer::Transport of the parsed and set transport
     *         mode
     */
    openvpn::DnsServer::Transport SetDNSTransport(GVariant *params);
#endif


  private:
    const ssize_t index = -1;
    bool enabled = false;
    bool prepare_removal = false;
    std::string device_name;
    Scope scope = DNS::Scope::GLOBAL;
    std::vector<std::string> name_servers;
    std::vector<std::string> search_domains;
    openvpn::DnsServer::Security dnssec_mode = openvpn::DnsServer::Security::Unset;
    openvpn::DnsServer::Transport dns_transport = openvpn::DnsServer::Transport::Unset;

    ResolverSettings(const ssize_t idx);
};
} // namespace DNS
} // namespace NetCfg
