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

#include <future>
#include <memory>
#include <string>
#include <asio.hpp>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/object/path.hpp>
#include <gdbuspp/proxy.hpp>

#include "netcfg/dns/systemd-resolved-ipaddr.hpp"


namespace NetCfg {
namespace DNS {
namespace resolved {

namespace Error {
/**
 *   Container for holding an internal error message
 */
struct Message
{
  using List = std::vector<Message>;

    /**
     *  Stores a new error message
     *
     *  @param method_   The D-Bus method involved causing this error
     *  @param message_  The error message itself
     */
    Message(const std::string &method_,
            const std::string &message_);

    std::string str() const;

    const std::string method;
    const std::string message;
};


/**
 *  Container for holding multiple error messages
 */


/**
 *  Main Error Storage object for handling internal
 *  Error messages across more interfaces/links
 */
class Storage
{
  public:
    using Ptr = std::shared_ptr<Storage>;

    [[nodiscard]] static Storage::Ptr Create();
    ~Storage() noexcept;

    /**
     *  Adds a new error message for a specific link device
     *
     *  @param link     Link name the error is related to
     *  @param method   D-Bus method where the error was caused
     *  @param message  The error message itself
     */
    void Add(const std::string &link, const std::string &method, const std::string &message);

    /**
     *  Reports all the links containing errors messages
     *
     *  @return std::vector<std::string> of all the link references with errors
     */
    std::vector<std::string> GetLinks() const;

    /**
     *  Returns the number of available errors for a specific link
     *
     *  @param link     Link name to lookup
     *
     *  @return uint32_t containing the number of stored error messages
     */
    size_t NumErrors(const std::string &link) const;

    /**
     *  Retrieves all the error messages for a specitic link.
     *
     *  Calling this will clear the error messages stored for
     *  that link
     *
     *  @param link     Link name to retrieve errors for
     *
     *  @return Error::List (std::vector<Error::Message>)
     */
    Error::Message::List GetErrors(const std::string &link);

  private:
    std::map<std::string, Error::Message::List> errors_;
    Storage();
};

} // namespace Error



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
     * @param asio_ctx   ASIO io_context where background D-Bus calls will
     *                   be executed
     * @param errors     Error::Storage to use for handling error messages
     * @param prx        DBus::Proxy::Client to use for communication
     * @param path       DBus::Object::Path to the interface in systemd-resolved
     * @param devname    std::string of the device name this is related to
     * @return Link::Ptr
     */
    [[nodiscard]] static Link::Ptr Create(asio::io_context &asio_ctx,
                                          Error::Storage::Ptr errors,
                                          DBus::Proxy::Client::Ptr prx,
                                          const DBus::Object::Path &path,
                                          const std::string &devname);
    ~Link() noexcept = default;

    /**
     *  Retrieve the D-Bus object path to the interface in systemd-resolved
     *
     * @return const DBus::Object::Path
     */
    const DBus::Object::Path GetPath() const;

    /**
     *  Retrieve the device name this link object is related to
     *
     * @return std::string
     */
    std::string GetDeviceName() const;

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
     * @return std::vector<std:string> of servers requested to be applied
     */
    std::vector<std::string> SetDNSServers(const IPAddress::List &servers);

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
     *
     * @param doms
     * @return  std::vector<std:string> of search domains requested to be applied
     */
    std::vector<std::string> SetDomains(const SearchDomain::List &doms);

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
     * @return False if this method is not available in the currently used
     *         org.freedesktop.resolve1 (systemd-resolved) service.
     */
    bool SetDefaultRoute(const bool route);

    /**
     *  Retrieve the DNSSEC mode for the interface
     *
     * @return std::string
     */
    std::string GetDNSSEC() const;

    /**
     *  Set the DNSSEC mode for the interface.
     *
     *  Valid modes are:
     *   - allow-downgrade  -  Opportunistic DNSSEC validation
     *   - yes              -  Enforce DNSSEC validation
     *   - no               -  Disable DNSSEC validation
     *
     * @param mode   std::string of the DNSSEC mode
     */
    void SetDNSSEC(const std::string &mode);

    /**
     *  Retrieve the DNS-over-TLS mode for the interface
     *
     *  The known values systemd-resolved uses (until version 252) are:
     *
     *  - yes            - All connections to the DNS server will be encrypted
     *  - no             - The connection to the DNS server will be unencrypted
     *  - opportunistic  - Connections to the server will be attempted to be
     *                     encrypted, but will fallback to unencrypted
     *
     *  For details of these modes, see the DNSOverTLS= setting in the
     *  resolved.conf(5) man page.
     *
     * @return std::string
     */
    std::string GetDNSOverTLS() const;

    /**
     *  Set the DNS-over-TLS mode for the interface
     *
     *  Valid mode values are:
     *
     *  - yes/true       - All connections to the DNS server will be encrypted
     *  - no/false       - The connection to the DNS server will be unencrypted
     *  - opportunistic  - Connections to the server will be attempted to be
     *                     encrypted, but will fallback to unencrypted
     *
     *  Not all values are available in all version of systemd-resolved.
     *  v245 is known to accept all, v239 can only use "no/false" or
     *  "opportunistic".
     *
     *  For details of these modes, see the DNSOverTLS= setting in the
     *  resolved.conf(5) man page.
     *
     * @param mode
     */
    void SetDNSOverTLS(const std::string &mode);

    /**
     *  Revert the DNS interface settings to the interface defaults, basically
     *  undoing any DNS settings set prior
     */
    void Revert();


    /**
     *  Retrieve all unprocessed error messages gathered in
     *  background calls.
     *
     *  @returns Error::List (std::vector<Message>) with
     *           all unprocessed error messages for this
     *           specific link.
     */
    Error::Message::List GetErrors() const;


  private:
    asio::io_context &asio_proxy;
    Error::Storage::Ptr errors;
    DBus::Proxy::Client::Ptr proxy = nullptr;
    DBus::Proxy::TargetPreset::Ptr tgt_link = nullptr;
    const std::string device_name;
    bool feature_set_default_route = true;

    Link(asio::io_context &asio_ctx,
         Error::Storage::Ptr errors,
         DBus::Proxy::Client::Ptr dbuscon,
         const DBus::Object::Path &path,
         const std::string &devname);


    /**
     *  Do a background D-Bus call to org.freedesktop.resolve1
     *  (systemd-resolved).  This will not provide any results
     *  back to the caller.
     *
     *  If a DBus::Proxy::Exception happens, the error message
     *  can be retrieved via the GetErrors() call.
     *
     *  @param method  std::string with the D-Bus method to call
     *  @param params  GVariant object containing all the arguments
     *                 to the D-Bus call
     *
     *  @throws resolved::Exception if the ASIO background worker
     *          thread is not running
     */
    void BackgroundCall(const std::string &method, GVariant *params = nullptr);
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
    ~Manager() noexcept;


    /**
     *  Retrieve a resolved::Link object for a specific network interface
     *
     * @param dev_name     std::string containing the network interface name
     *
     * @return Link::Ptr to the new Link object to the network interface
     */
    Link::Ptr RetrieveLink(const std::string &dev_name);

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
    bool asio_keep_running = false;
    std::future<void> async_proxy_thread;
    asio::io_context asio_proxy;
    Error::Storage::Ptr asio_errors = nullptr;

    Manager(DBus::Connection::Ptr conn);
};
} // namespace resolved
} // namespace DNS
} // namespace NetCfg
