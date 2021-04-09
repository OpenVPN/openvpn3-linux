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

#ifndef OPENVPN3_DBUS_CONSTANTS_HPP
#define OPENVPN3_DBUS_CONSTANTS_HPP

#include <cstdint>
#include <string>
#include <array>

/*
 *  Various D-Bus bus names, root paths and interface names used
 *  for the IPC communication between the various componets
 *
 */

/* Logger service */
const std::string OpenVPN3DBus_name_log = "net.openvpn.v3.log";
const std::string OpenVPN3DBus_rootp_log = "/net/openvpn/v3/log";
const std::string OpenVPN3DBus_interf_log = "net.openvpn.v3.log";


/* Configuration Manager */
const std::string OpenVPN3DBus_name_configuration = "net.openvpn.v3.configuration";
const std::string OpenVPN3DBus_rootp_configuration = "/net/openvpn/v3/configuration";
const std::string OpenVPN3DBus_interf_configuration = "net.openvpn.v3.configuration";

/* Session manager */
const std::string OpenVPN3DBus_name_sessions = "net.openvpn.v3.sessions";
const std::string OpenVPN3DBus_rootp_sessions = "/net/openvpn/v3/sessions";
const std::string OpenVPN3DBus_interf_sessions = "net.openvpn.v3.sessions";

/* Backend manager interface -> session manager's interface to start and
 * communicate with VPN client backends
 */
const std::string OpenVPN3DBus_name_backends = "net.openvpn.v3.backends";
const std::string OpenVPN3DBus_rootp_backends = "/net/openvpn/v3/backends";
const std::string OpenVPN3DBus_interf_backends = "net.openvpn.v3.backends";
const std::string OpenVPN3DBus_interf_backends_manager = OpenVPN3DBus_interf_backends + ".manager";


/* Backend VPN client process (openvpn-service-client) - which is the real tunnel instance */
const std::string OpenVPN3DBus_name_backends_be = "net.openvpn.v3.backends.be";
const std::string OpenVPN3DBus_rootp_backends_session =  OpenVPN3DBus_rootp_backends + "/session";
const std::string OpenVPN3DBus_rootp_backends_manager = OpenVPN3DBus_rootp_backends + "/manager";


/* Network Configuration Service
 * Creates/destroys tun devices, configures IP addresses, routing, DNS
 * and other network configuration related tasks
 */
const std::string OpenVPN3DBus_name_netcfg = "net.openvpn.v3.netcfg";
const std::string OpenVPN3DBus_rootp_netcfg = "/net/openvpn/v3/netcfg";
const std::string OpenVPN3DBus_interf_netcfg = "net.openvpn.v3.netcfg";


/**
 *  Status - major codes
 *  These codes represents a type of master group
 *  the status code belongs to.
 */

const uint8_t StatusMajorCount = 6;
enum class StatusMajor : std::uint8_t {
        UNSET,                      /**< Invalid status major code, used for initialization */
        CONFIG,                     /**< Status is related to configuration */
        CONNECTION,                 /**< Status is related to an OpenVPN connection */
        SESSION,                    /**< Status is related to an on-going session */
        PKCS11,                     /**< Status is related to Smart Card/PKCS#11 operations */
        PROCESS                     /**< Status is related to process management */
 };

const std::array<const std::string, StatusMajorCount> StatusMajor_str = {{
        "(unset)",
        "Configuration",
        "Connection",
        "Session",
        "PKCS#11",
        "Process"
}};

const uint8_t StatusMinorCount = 30;
enum class  StatusMinor : std::uint16_t {
        UNSET,                       /**< An invalid result code, used for initialization */

        CFG_ERROR,                   /**< Failed parsing configuration */
        CFG_OK,                      /**< Configuration file parsed successfully */
        CFG_INLINE_MISSING,          /**< Some embedded (inline) files are missing */
        CFG_REQUIRE_USER,            /**< Require input from user */

        CONN_INIT,                   /**< Client connection initialized, ready to connect */
        CONN_CONNECTING,             /**< Client started connecting */
        CONN_CONNECTED,              /**< Client have connected successfully */
        CONN_DISCONNECTING,          /**< Client started disconnect process */
        CONN_DISCONNECTED,           /**< Client completed disconnecting */
        CONN_FAILED,                 /**< Client connection failed, disconnected */
        CONN_AUTH_FAILED,            /**< Client authentication failed, disconnected */
        CONN_RECONNECTING,           /**< Client needed to reconnect */
        CONN_PAUSING,                /**< Client started to pause the connection */
        CONN_PAUSED,                 /**< Client connection is paused */
        CONN_RESUMING,               /**< Client connection is resuming */
        CONN_DONE,                   /**< Client connection process have completed and exited successfully */

        SESS_NEW,                    /**< New session object created */
        SESS_BACKEND_COMPLETED,      /**< Backend session object have completed its task */
        SESS_REMOVED,                /**< Session object removed */
        SESS_AUTH_USERPASS,          /**< User/password authentication needed */
        SESS_AUTH_CHALLENGE,         /**< Challenge/response authentication needed */
        SESS_AUTH_URL,               /**< Authentication needed via external URL */

        PKCS11_SIGN,                 /**< PKCS#11 sign operation required */
        PKCS11_ENCRYPT,              /**< PKCS#11 encryption operation required */
        PKCS11_DECRYPT,              /**< PKCS#11 decryption operation required */
        PKCS11_VERIFY,               /**< PKCS#11 verification operation required */

        PROC_STARTED,                /**< Successfully started a new process */
        PROC_STOPPED,                /**< A process of ours stopped as expected */
        PROC_KILLED,                 /**< A process of ours stopped unexpectedly */
};

const std::array<const std::string, StatusMinorCount> StatusMinor_str = {{
        "(unset)",

        "Configuration error",
        "Configuration OK",
        "Configuration missing inline data",
        "Configuration requires user input",

        "Client initialized",
        "Client connecting",
        "Client connected",
        "Client disconnecting",
        "Client disconnected",
        "Client connection failed",
        "Client authentication failed",
        "Client reconnect",
        "Client pausing connection",
        "Client connection paused",
        "Client connection resuming",
        "Client process exited",

        "New session created",
        "Backend Session Object completed",
        "Session deleted",
        "User/password authentication",
        "Challenge/response authentication",
        "URL authentication",

        "PKCS#11 Sign",
        "PKCS#11 Encrypt",
        "PKCS#11 Decrypt",
        "PKCS#11 Verify",

        "Process started",
        "Process stopped",
        "Process killed"
}};


const uint8_t ClientAttentionTypeCount = 4;
enum class ClientAttentionType : std::uint8_t {
    UNSET,
    CREDENTIALS,
    PKCS11,
    ACCESS_PERM
};

const std::array<const std::string, ClientAttentionTypeCount> ClientAttentionType_str = {{
    "(unset)",
    "User Credentials",
    "PKCS#11 operation",
    "Requesting access permission"
}};

const uint8_t ClientAttentionGroupCount = 9;
enum class ClientAttentionGroup : std::uint8_t {
    UNSET,
    USER_PASSWORD,
    HTTP_PROXY_CREDS,
    PK_PASSPHRASE,
    CHALLENGE_STATIC,
    CHALLENGE_DYNAMIC,
    PKCS11_SIGN,
    PKCS11_DECRYPT,
    OPEN_URL
};

const std::array<const std::string, ClientAttentionGroupCount> ClientAttentionGroup_str = {{
    "(unset)",
    "Username/password authentication",
    "HTTP proxy credentials",
    "Private key passphrase",
    "Static challenge",
    "Dynamic challenge",
    "PKCS#11 sign operation",
    "PKCS#11 decrypt operation",
    "Web authentication"
}};

#endif
