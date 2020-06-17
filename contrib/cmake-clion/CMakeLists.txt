# This file is supposed to be linked from the root of openvpn3-linux
# ln -s contrib/cmake-clion/CMakeLists.txt
# You should still run ./configure to generate a valid config.h before
# utilizing CMake

cmake_minimum_required(VERSION 3.10)
project(openvpn3_linux C CXX)
set(CMAKE_CXX_STANDARD 14)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

find_library(UUID uuid)

pkg_check_modules(JSONCPP REQUIRED jsoncpp)
pkg_check_modules(GLIB2 REQUIRED glib-2.0)
pkg_check_modules(GIO2 REQUIRED gio-2.0)

option(ENABLE_OVPNDCO "Enable OpenVPN data channel offloading")

set(OPENVPN3_LINUX_CONFIG_DEFINES
        -DUSE_OPENSSL
        -DUSE_ASIO
        -DASIO_STANDALONE
        -DOPENVPN_USERNAME="openvpn"
        -DLIBEXECDIR="/usr/libexec"
    )
if (${ENABLE_OVPNDCO})
    list(APPEND OPENVPN3_LINUX_CONFIG_DEFINES -DENABLE_OVPNDCO)
endif()
add_definitions(${OPENVPN3_LINUX_CONFIG_DEFINES})

set(OPENVPN3_LINUX_LIBS
        ${GLIB2_LDFLAGS}
        ${GIO2_LDFLAGS}
        ${CMAKE_THREAD_LIBS_INIT}
        ${OPENSSL_LIBRARIES}
        ${JSONCPP_LIBRARIES}
        ${UUID})

if (${ENABLE_OVPNDCO})
    find_package(PkgConfig)
    pkg_search_module(LIBNL REQUIRED libnl-genl-3.0)
    list(APPEND OPENVPN3_LINUX_LIBS ${LIBNL_LIBRARIES})
endif()

find_package(Protobuf REQUIRED)

link_libraries(${OPENVPN3_LINUX_LIBS})

if(NOT DEFINED OPENVPN3_LINUX_ROOT)
    set(OPENVPN3_LINUX_ROOT ${CMAKE_SOURCE_DIR})
endif()

set(OPENVPN3_LINUX_INCLUDE_DIRS
        ${OPENVPN3_LINUX_ROOT}
        ${OPENVPN3_LINUX_ROOT}/openvpn3-core
        ${OPENVPN3_LINUX_ROOT}/src
        ${GLIB2_INCLUDE_DIRS}
        ${JSONCPP_INCLUDE_DIRS}
        ${OPENVPN3_LINUX_ROOT}/vendor/asio/asio/include)

if (${ENABLE_OVPNDCO})
    find_path(
        LIBNL_INCLUDE_DIR
        PATH_SUFFIXES
            include/libnl3
        NAMES
            netlink/version.h
    )
    list(APPEND OPENVPN3_LINUX_INCLUDE_DIRS
        ${LIBNL_INCLUDE_DIR}
        ${OPENVPN3_LINUX_ROOT}/ovpn-dco/include/uapi
        ${CMAKE_BINARY_DIR}
        ${CMAKE_BINARY_DIR}/src/netcfg # for protobuf generated files
        )
endif()
include_directories(${OPENVPN3_LINUX_INCLUDE_DIRS})

get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
    set(OPENVPN3_LINUX_CONFIG_DEFINES
            ${OPENVPN3_LINUX_CONFIG_DEFINES} PARENT_SCOPE)
    set(OPENVPN3_LINUX_LIBS ${OPENVPN3_LINUX_LIBS} PARENT_SCOPE)
    set(OPENVPN3_LINUX_INCLUDE_DIRS ${OPENVPN3_LINUX_INCLUDE_DIRS} PARENT_SCOPE)
endif()


add_executable(openvpn3-service-client
        src/client/openvpn3-service-client.cpp
        src/client/core-client.hpp
        src/client/core-client-netcfg.hpp
        src/client/backend-signals.hpp
        src/client/statistics.hpp
        src/client/statusevent.hpp
        src/common/core-extensions.hpp
        src/common/cmdargparser.cpp
        src/common/requiresqueue.cpp
        src/common/timestamp.cpp
        src/common/utils.cpp
        src/common/cmdargparser.cpp
        src/common/configfileparser.cpp
        src/configmgr/proxy-configmgr.hpp
        src/configmgr/overrides.cpp
        src/log/dbus-log.hpp
        src/log/proxy-log.hpp
        src/netcfg/proxy-netcfg.hpp
        src/netcfg/proxy-netcfg.cpp
        src/netcfg/dco-keyconfig.proto
        )
protobuf_generate(TARGET openvpn3-service-client)
target_link_libraries(openvpn3-service-client lz4 ${PROTOBUF_LIBRARIES})

add_executable(openvpn3-service-configmgr
        src/configmgr/openvpn3-service-configmgr.cpp
        src/configmgr/configmgr.hpp
        src/configmgr/overrides.cpp
        src/common/cmdargparser.hpp
        src/common/cmdargparser.cpp
        src/common/core-extensions.hpp
        src/common/lookup.cpp
        src/common/timestamp.cpp
        src/common/utils.cpp
        src/common/cmdargparser.cpp
        src/common/configfileparser.cpp
        src/log/dbus-log.hpp
        )

add_executable(openvpn3
        src/ovpn3cli/openvpn3.cpp
        src/ovpn3cli/arghelpers.cpp
        src/ovpn3cli/commands/version.cpp
        src/ovpn3cli/commands/config.cpp
        src/ovpn3cli/commands/session.cpp
        src/ovpn3cli/commands/log.cpp
        src/ovpn3cli/arghelpers.hpp
        src/common/lookup.cpp
        src/common/lookup.hpp
        src/common/cmdargparser.cpp
        src/configmgr/proxy-configmgr.hpp
        src/configmgr/overrides.hpp
        src/configmgr/overrides.cpp
        src/sessionmgr/proxy-sessionmgr.hpp
        src/sessionmgr/sessionmgr-events.hpp
        src/sessionmgr/sessionmgr-events.cpp
        src/dbus/requiresqueue-proxy.hpp
        src/common/cmdargparser.hpp
        src/common/cmdargparser.cpp
        src/common/requiresqueue.cpp
        src/common/timestamp.cpp
        src/common/utils.cpp
        src/common/configfileparser.cpp
        )
target_link_libraries(openvpn3 lz4)

add_executable(openvpn3-service-netcfg
        src/netcfg/openvpn3-service-netcfg.cpp
        src/netcfg/core-tunbuilder.cpp
        src/netcfg/netcfg.hpp
        src/netcfg/netcfg-device.hpp
        src/netcfg/netcfg-dco.hpp
        src/netcfg/netcfg-dco.cpp
        src/netcfg/netcfg-exception.hpp
        src/netcfg/netcfg-options.hpp
        src/netcfg/netcfg-signals.hpp
        src/netcfg/netcfg-changeevent.hpp
        src/netcfg/netcfg-subscriptions.hpp
        src/netcfg/netcfg-subscriptions.cpp
        src/netcfg/dns/resolvconf-file.cpp
        src/netcfg/dns/resolver-settings.cpp
        src/netcfg/dns/settings-manager.cpp
        src/netcfg/dns/systemd-resolved.cpp
        src/netcfg/dns/proxy-systemd-resolved.cpp
        src/netcfg/core-tunbuilder.hpp
        src/common/cmdargparser.cpp
        src/common/lookup.cpp
        src/common/requiresqueue.cpp
        src/common/timestamp.cpp
        src/common/utils.cpp
        src/common/cmdargparser.cpp
        src/common/configfileparser.cpp
        src/log/core-dbus-logbase.hpp
        src/log/dbus-log.hpp
        src/netcfg/netcfg-changeevent.cpp
        src/netcfg/dco-keyconfig.proto
        )

find_library(CAPNG cap-ng)
protobuf_generate(TARGET openvpn3-service-netcfg)
target_link_libraries(openvpn3-service-netcfg ${CAPNG} ${PROTOBUF_LIBRARIES})

add_executable(openvpn3-service-logger
        src/log/openvpn3-service-logger.cpp
        src/log/ansicolours.hpp
        src/log/colourengine.hpp
        src/log/dbus-log.hpp
        src/log/log-helpers.hpp
        src/log/logevent.hpp
        src/log/logger.hpp
        src/log/logwriter.hpp
        src/log/service.hpp
        src/common/timestamp.cpp
        src/common/utils.cpp
        src/common/cmdargparser.cpp
        src/common/configfileparser.cpp
        )

add_executable(openvpn3-service-sessionmgr
        src/sessionmgr/openvpn3-service-sessionmgr.cpp
        src/sessionmgr/sessionmgr.hpp
        src/sessionmgr/sessionmgr-events.hpp
        src/sessionmgr/sessionmgr-events.cpp
        src/client/statusevent.hpp
        src/common/cmdargparser.cpp
        src/common/lookup.cpp
        src/common/requiresqueue.cpp
        src/common/timestamp.cpp
        src/common/utils.cpp
        src/common/cmdargparser.cpp
        src/common/configfileparser.cpp
        src/log/dbus-log.hpp
        )

add_executable(openvpn3-service-backendstart
        src/client/openvpn3-service-backendstart.cpp
        src/client/statusevent.hpp
        src/common/cmdargparser.cpp
        src/common/lookup.cpp
        src/common/requiresqueue.cpp
        src/common/timestamp.cpp
        src/common/utils.cpp
        src/common/cmdargparser.cpp
        src/common/configfileparser.cpp
        src/log/dbus-log.hpp
        )

add_executable(cli-netcfg
        src/tests/netcfg/cli.cpp
        src/netcfg/netcfg.hpp
        src/netcfg/netcfg-device.hpp
        src/netcfg/netcfg-exception.hpp
        src/netcfg/netcfg-options.hpp
        src/netcfg/proxy-netcfg.hpp
        src/netcfg/proxy-netcfg.cpp
        src/netcfg/netcfg-signals.hpp
        src/netcfg/netcfg-changeevent.hpp
        src/netcfg/netcfg-subscriptions.hpp
        src/netcfg/netcfg-subscriptions.cpp
        src/netcfg/core-tunbuilder.hpp
        src/common/lookup.cpp
        src/common/requiresqueue.cpp
        src/common/timestamp.cpp
        src/common/utils.cpp
        src/log/core-dbus-logbase.hpp
        src/log/dbus-log.hpp
        src/netcfg/dco-keyconfig.proto
        )
target_link_libraries(cli-netcfg lz4 ${PROTOBUF_LIBRARIES})
protobuf_generate(TARGET cli-netcfg)

add_subdirectory(addons/aws)
