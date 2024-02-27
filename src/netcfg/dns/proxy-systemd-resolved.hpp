//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2020-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   proxy-systemd-resolved.hpp
 *
 * @brief  D-Bus proxy for the systemd-resolved service
 */

#pragma once

#include <exception>
#include <memory>
#include <string>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/object/path.hpp>
#include <gdbuspp/proxy.hpp>


namespace NetCfg {
namespace DNS {
namespace resolved {


/**
 *  Helper struct to manage a resolver record used by the systemd-resolved
 *  service.
 *
 *  This is capable of converting between the D-Bus data types the D-Bus
 *  service uses and C++ data types.
 */
struct ResolverRecord
{
    /**
     * Defines a ResolverRecord::List as an alias to std::vector<ResolverRecord>
     */
    using List = std::vector<ResolverRecord>;

    /**
     *  Registers a new resolver record
     *
     * @param family  IPv4/IPv6 IP address identifier
     * @param server  std::string containing the DNS server IP address
     */
    ResolverRecord(const unsigned short family, const std::string &server);

    /**
     *  Registers a new resolver record, parsing a GVariant* D-Bus value
     *  response from the systemd-resolved
     *
     * @param entry   GVariant object containing the D-Bus values to parse
     */
    ResolverRecord(GVariant *entry);

    ~ResolverRecord() = default;

    /**
     *  Retrieve the data stored in this struct as a D-Bus value object
     *  which can be passed directly to the systemd-resolved service
     *
     * @return GVariant*  D-Bus value object containing all the information
     */
    GVariant *GetGVariant() const;

    unsigned short family; ///< IPv4/IPv6 identifier
    std::string server;    ///< IP address as a string to the DNS server
};


/**
 *  Helper struct to manage search domain records used by the systemd-resolved
 *  service.
 *
 *  This is capable of converting between the D-Bus data types the D-Bus
 *  service uses and C++ data types.
 */
struct SearchDomain
{
    /**
     * Defines a SearchDomain::List as an alias to std::vector<SearchDomain>
     */
    using List = std::vector<SearchDomain>;

    /**
     *  Registers a new Search Domain record
     *
     * @param domain   std::string containing the DNS search domain
     * @param route    bool flag identifying if it is a "routing domain"
     *                 See https://systemd.io/RESOLVED-VPNS/ for details
     */
    SearchDomain(const std::string &domain, const bool route);

    /**
     *  Registers a new resolver record, parsing a GVariant* D-Bus value
     *  response from the systemd-resolved
     *
     * @param entry   GVariant object containing the D-Bus values to parse
     */
    SearchDomain(GVariant *entry);

    ~SearchDomain() = default;

    /**
     *  Retrieve the data stored in this struct as a D-Bus value object
     *  which can be passed directly to the systemd-resolved service
     *
     * @return GVariant*  D-Bus value object containing all the information
     */
    GVariant *GetGVariant() const;

    std::string search{};
    bool routing = false;
};



class Exception : public std::exception
{
  public:
    Exception(const std::string &err)
        : errmsg(err)
    {
    }
    virtual ~Exception() = default;

    virtual const char *what() const noexcept
    {
        return errmsg.c_str();
    }

  private:
    std::string errmsg;
};



/**
 *  This represents a single Link object provided by the systemd-resolved
 *  service.  This is a proxy object to interact with the D-Bus object
 *  as a C++ object.
 *
 *  There is a Link object available per available network interface
 *
 *  The Link object is commonly created by the resolved::Manager object, which
 *  has the needed lookup code to retrieve the proper D-Bus path for the
 *  interface.
 */
class Link
{
  public:
    using Ptr = std::shared_ptr<Link>;

    /**
     *  Prepare a new proxy object for a Link/network interface
     *
     * @param prx        DBus::Proxy::Client to use for communication
     * @param path       DBus::Object::Path to the interface in systemd-resolved
     * @return Link::Ptr
     */
    [[nodiscard]] static Link::Ptr Create(DBus::Proxy::Client::Ptr prx,
                                          const DBus::Object::Path &path);
    ~Link() noexcept = default;

    /**
     *  Retrieve the D-Bus object path to the interface in systemd-resolved
     *
     * @return const DBus::Object::Path
     */
    const DBus::Object::Path GetPath() const;

    /**
     *  Retrieve a list of DNS servers configured for this interface
     *
     * @return const std::vector<std::string>
     */
    const std::vector<std::string> GetDNSServers() const;

    /**
     *  Replace the currently configured list of DNS servers with a new
     *  list of servers
     *
     * @param servers ResolverRecord::List with all servers to use
     */
    void SetDNSServers(const ResolverRecord::List &servers) const;

    /**
     *  Retrieve the DNS server currently being used for DNS queries
     *
     * @return const std::string
     */
    const std::string GetCurrentDNSServer() const;

    /**
     *  Retrieve a list of all search domains configured for this interface
     *
     * @return const SearchDomain::List
     */
    const SearchDomain::List GetDomains() const;

    /**
     *  Replace the currently used list of DNS search domains with a new
     *  list
     * @param doms
     */
    void SetDomains(const SearchDomain::List &doms) const;

    /**
     *  Retrieve the "Default Route" flag for the interface.
     *
     *  See https://systemd.io/RESOLVED-VPNS/ for details
     *
     *  Note: This feature is not available in all systemd-resolved versions
     *
     * @return true   Any DNS lookups for which no matching routing or
     *                search domains are defined are routed to this interface
     * @return false  Will only do DNS lookup for configured search domains
     *                on this interface
     */
    bool GetDefaultRoute() const;

    /**
     *  Set the "Default Route" flag for the interface.
     *
     *  See https://systemd.io/RESOLVED-VPNS/ for details
     *
     *  Note: This feature is not available in all systemd-resolved versions
     *
     * @param route   bool value to enable/disable the DNS default route flag.
     *                See @GetDefaultRoute() return values for more details
     */
    void SetDefaultRoute(const bool route) const;


    /**
     *  Revert the DNS interface settings to the interface defaults, basically
     *  undoing any DNS settings set prior
     */
    void Revert() const;

  private:
    DBus::Proxy::Client::Ptr proxy = nullptr;
    DBus::Proxy::TargetPreset::Ptr tgt_link = nullptr;

    Link(DBus::Proxy::Client::Ptr dbuscon, const DBus::Object::Path &path);
};



/**
 *  Main manager interface to the systemd-resolved D-Bus service
 *
 *  This is the starting point to retrieve a resolved::Link object for
 *  network interfaces.
 *
 */
class Manager
{
  public:
    using Ptr = std::shared_ptr<Manager>;

    /**
     *  Create a new Manager object used to interact with systemd-resolved
     *
     * @param conn           DBus::Connection::Ptr to the D-Bus bus to use
     *
     * @return Manager::Ptr (shared_ptr) to the new Manager object
     */
    [[nodiscard]] static Manager::Ptr Create(DBus::Connection::Ptr conn);
    ~Manager() noexcept = default;


    /**
     *  Retrieve a resolved::Link object for a specific network interface
     *
     * @param dev_name     std::string containing the network interface name
     *
     * @return Link::Ptr to the new Link object to the network interface
     */
    Link::Ptr RetrieveLink(const std::string dev_name) const;

    /**
     *  Retrieve the D-Bus path to a link object used by the systemd-resolved
     *  service
     *
     * @param if_idx  unsigned int of the interface index ("if index") to lookup
     *
     * @return DBus::Object::Path used by the systemd-resolved to the network
     *         interface
     */
    DBus::Object::Path GetLink(unsigned int if_idx) const;

  private:
    DBus::Proxy::Client::Ptr proxy = nullptr;
    DBus::Proxy::TargetPreset::Ptr tgt_resolved = nullptr;

    Manager(DBus::Connection::Ptr conn);
};
} // namespace resolved
} // namespace DNS
} // namespace NetCfg
