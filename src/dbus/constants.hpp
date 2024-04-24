//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//  Copyright (C)  Lev Stipakov <lev@openvpn.net>
//  Copyright (C)  Heiko Hund <heiko@openvpn.net>
//

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <gdbuspp/glib2/utils.hpp>

/*
 *  Various D-Bus bus names, root paths and interface names used
 *  for the IPC communication between the various components
 *
 */

namespace Constants {

namespace Base {
constexpr std::string_view BUSNAME{"net.openvpn.v3."};
constexpr std::string_view ROOT_PATH{"/net/openvpn/v3/"};
constexpr std::string_view INTERFACE{"net.openvpn.v3."};
} // namespace Base

#include <gdbuspp/gen-constants.hpp>

} // namespace Constants


/**
 *  Status - major codes
 *  These codes represents a type of master group
 *  the status code belongs to.
 */

const uint8_t StatusMajorCount = 6;
enum class StatusMajor : std::uint8_t
{
    UNSET,      /**< Invalid status major code, used for initialization */
    CONFIG,     /**< Status is related to configuration */
    CONNECTION, /**< Status is related to an OpenVPN connection */
    SESSION,    /**< Status is related to an on-going session */
    PKCS11,     /**< Status is related to Smart Card/PKCS#11 operations */
    PROCESS     /**< Status is related to process management */
};

const std::array<const std::string, StatusMajorCount> StatusMajor_str = {
    {"(unset)",
     "Configuration",
     "Connection",
     "Session",
     "PKCS#11",
     "Process"}};


template <>
inline const char *glib2::DataType::DBus<StatusMajor>() noexcept
{
    return "u";
}

template <>
inline StatusMajor glib2::Value::Get<StatusMajor>(GVariant *v) noexcept
{
    return static_cast<StatusMajor>(glib2::Value::Get<uint32_t>(v));
}

template <>
inline StatusMajor glib2::Value::Extract<StatusMajor>(GVariant *v, int elm) noexcept
{
    return static_cast<StatusMajor>(glib2::Value::Extract<uint32_t>(v, elm));
}

inline std::ostream &operator<<(std::ostream &os, const StatusMajor &obj)
{
    return os << static_cast<uint32_t>(obj);
}



const uint8_t StatusMinorCount = 30;
enum class StatusMinor : std::uint16_t
{
    UNSET, /**< An invalid result code, used for initialization */

    CFG_ERROR,          /**< Failed parsing configuration */
    CFG_OK,             /**< Configuration file parsed successfully */
    CFG_INLINE_MISSING, /**< Some embedded (inline) files are missing */
    CFG_REQUIRE_USER,   /**< Require input from user */

    CONN_INIT,          /**< Client connection initialized, ready to connect */
    CONN_CONNECTING,    /**< Client started connecting */
    CONN_CONNECTED,     /**< Client have connected successfully */
    CONN_DISCONNECTING, /**< Client started disconnect process */
    CONN_DISCONNECTED,  /**< Client completed disconnecting */
    CONN_FAILED,        /**< Client connection failed, disconnected */
    CONN_AUTH_FAILED,   /**< Client authentication failed, disconnected */
    CONN_RECONNECTING,  /**< Client needed to reconnect */
    CONN_PAUSING,       /**< Client started to pause the connection */
    CONN_PAUSED,        /**< Client connection is paused */
    CONN_RESUMING,      /**< Client connection is resuming */
    CONN_DONE,          /**< Client connection process have completed and exited successfully */

    SESS_NEW,               /**< New session object created */
    SESS_BACKEND_COMPLETED, /**< Backend session object have completed its task */
    SESS_REMOVED,           /**< Session object removed */
    SESS_AUTH_USERPASS,     /**< User/password authentication needed */
    SESS_AUTH_CHALLENGE,    /**< Challenge/response authentication needed */
    SESS_AUTH_URL,          /**< Authentication needed via external URL */

    PKCS11_SIGN,    /**< PKCS#11 sign operation required */
    PKCS11_ENCRYPT, /**< PKCS#11 encryption operation required */
    PKCS11_DECRYPT, /**< PKCS#11 decryption operation required */
    PKCS11_VERIFY,  /**< PKCS#11 verification operation required */

    PROC_STARTED, /**< Successfully started a new process */
    PROC_STOPPED, /**< A process of ours stopped as expected */
    PROC_KILLED,  /**< A process of ours stopped unexpectedly */
};


const std::array<const std::string, StatusMinorCount> StatusMinor_str = {
    {"(unset)",

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
     "Process killed"}};


template <>
inline const char *glib2::DataType::DBus<StatusMinor>() noexcept
{
    return "u";
}

template <>
inline StatusMinor glib2::Value::Get<StatusMinor>(GVariant *v) noexcept
{
    return static_cast<StatusMinor>(glib2::Value::Get<uint32_t>(v));
}

template <>
inline StatusMinor glib2::Value::Extract<StatusMinor>(GVariant *v, int elm) noexcept
{
    return static_cast<StatusMinor>(glib2::Value::Extract<uint32_t>(v, elm));
}

inline std::ostream &operator<<(std::ostream &os, const StatusMinor &obj)
{
    return os << static_cast<uint32_t>(obj);
}


const uint8_t ClientAttentionTypeCount = 4;
enum class ClientAttentionType : std::uint8_t
{
    UNSET,
    CREDENTIALS,
    PKCS11,
    ACCESS_PERM
};


const std::array<const std::string, ClientAttentionTypeCount> ClientAttentionType_str = {
    {"(unset)",
     "User Credentials",
     "PKCS#11 operation",
     "Requesting access permission"}};

template <>
inline const char *glib2::DataType::DBus<ClientAttentionType>() noexcept
{
    return "u";
}

template <>
inline ClientAttentionType glib2::Value::Get<ClientAttentionType>(GVariant *v) noexcept
{
    return static_cast<ClientAttentionType>(glib2::Value::Get<uint32_t>(v));
}

template <>
inline ClientAttentionType glib2::Value::Extract<ClientAttentionType>(GVariant *v, int elm) noexcept
{
    return static_cast<ClientAttentionType>(glib2::Value::Extract<uint32_t>(v, elm));
}

inline std::ostream &operator<<(std::ostream &os, const ClientAttentionType &obj)
{
    return os << static_cast<uint32_t>(obj);
}


const uint8_t ClientAttentionGroupCount = 10;
enum class ClientAttentionGroup : std::uint8_t
{
    UNSET,
    USER_PASSWORD,
    HTTP_PROXY_CREDS,
    PK_PASSPHRASE,
    CHALLENGE_STATIC,
    CHALLENGE_DYNAMIC,
    CHALLENGE_AUTH_PENDING,
    PKCS11_SIGN,
    PKCS11_DECRYPT,
    OPEN_URL
};


const std::array<const std::string, ClientAttentionGroupCount> ClientAttentionGroup_str = {
    {"(unset)",
     "Username/password authentication",
     "HTTP proxy credentials",
     "Private key passphrase",
     "Static challenge",
     "Dynamic challenge",      // CHALLENGE_DYNAMIC
     "Pending authentication", // AUTH_PENDING
     "PKCS#11 sign operation",
     "PKCS#11 decrypt operation",
     "Web authentication"}};


template <>
inline const char *glib2::DataType::DBus<ClientAttentionGroup>() noexcept
{
    return "u";
}

template <>
inline ClientAttentionGroup glib2::Value::Get<ClientAttentionGroup>(GVariant *v) noexcept
{
    return static_cast<ClientAttentionGroup>(glib2::Value::Get<uint32_t>(v));
}

template <>
inline ClientAttentionGroup glib2::Value::Extract<ClientAttentionGroup>(GVariant *v, int elm) noexcept
{
    return static_cast<ClientAttentionGroup>(glib2::Value::Extract<uint32_t>(v, elm));
}

inline std::ostream &operator<<(std::ostream &os, const ClientAttentionGroup &obj)
{
    return os << static_cast<uint32_t>(obj);
}
