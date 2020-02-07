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

set(OPENVPN3_LINUX_CONFIG_DEFINES
        -DUSE_OPENSSL
        -DUSE_ASIO
        -DASIO_STANDALONE
        -DOPENVPN_USERNAME="openvpn"
        -DPACKAGE_GUIVERSION="2"
        -DOPENVPN_GROUP="openvpn"
        -DPACKAGE_NAME="openvpn3-linux")
add_definitions(${OPENVPN3_LINUX_CONFIG_DEFINES})

set(OPENVPN3_LINUX_LIBS
        ${GLIB2_LDFLAGS}
        ${GIO2_LDFLAGS}
        ${CMAKE_THREAD_LIBS_INIT}
        ${OPENSSL_LIBRARIES}
        ${JSONCPP_LIBRARIES}
        ${UUID})
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
include_directories(${OPENVPN3_LINUX_INCLUDE_DIRS})

get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
    set(OPENVPN3_LINUX_CONFIG_DEFINES
            ${OPENVPN3_LINUX_CONFIG_DEFINES} PARENT_SCOPE)
    set(OPENVPN3_LINUX_LIBS ${OPENVPN3_LINUX_LIBS} PARENT_SCOPE)
    set(OPENVPN3_LINUX_INCLUDE_DIRS ${OPENVPN3_LINUX_INCLUDE_DIRS} PARENT_SCOPE)
endif()

file(WRITE config.h "")

add_executable(openvpn3-service-client
        ${OPENVPN3_LINUX_ROOT}/src/client/openvpn3-service-client.cpp
        ${OPENVPN3_LINUX_ROOT}/src/client/core-client.hpp
        ${OPENVPN3_LINUX_ROOT}/src/client/core-client-netcfg.hpp
        ${OPENVPN3_LINUX_ROOT}/src/client/backend-signals.hpp
        ${OPENVPN3_LINUX_ROOT}/src/client/statistics.hpp
        ${OPENVPN3_LINUX_ROOT}/src/client/statusevent.hpp
        ${OPENVPN3_LINUX_ROOT}/src/common/core-extensions.hpp
        ${OPENVPN3_LINUX_ROOT}/src/common/cmdargparser.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/requiresqueue.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/timestamp.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/utils.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/cmdargparser.cpp
        ${OPENVPN3_LINUX_ROOT}/src/configmgr/proxy-configmgr.hpp
        ${OPENVPN3_LINUX_ROOT}/src/configmgr/overrides.cpp
        ${OPENVPN3_LINUX_ROOT}/src/log/dbus-log.hpp
        ${OPENVPN3_LINUX_ROOT}/src/log/proxy-log.hpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/proxy-netcfg.hpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/proxy-netcfg.cpp
        )

add_executable(openvpn3-service-configmgr
        ${OPENVPN3_LINUX_ROOT}/src/configmgr/openvpn3-service-configmgr.cpp
        ${OPENVPN3_LINUX_ROOT}/src/configmgr/configmgr.hpp
        ${OPENVPN3_LINUX_ROOT}/src/configmgr/overrides.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/cmdargparser.hpp
        ${OPENVPN3_LINUX_ROOT}/src/common/cmdargparser.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/core-extensions.hpp
        ${OPENVPN3_LINUX_ROOT}/src/common/lookup.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/timestamp.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/utils.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/cmdargparser.cpp
        ${OPENVPN3_LINUX_ROOT}/src/log/dbus-log.hpp
        )

add_executable(openvpn3
        ${OPENVPN3_LINUX_ROOT}/src/ovpn3cli/openvpn3.cpp
        ${OPENVPN3_LINUX_ROOT}/src/ovpn3cli/arghelpers.cpp
        ${OPENVPN3_LINUX_ROOT}/src/ovpn3cli/commands/version.cpp
        ${OPENVPN3_LINUX_ROOT}/src/ovpn3cli/commands/config.cpp
        ${OPENVPN3_LINUX_ROOT}/src/ovpn3cli/commands/session.cpp
        ${OPENVPN3_LINUX_ROOT}/src/ovpn3cli/commands/log.cpp
        ${OPENVPN3_LINUX_ROOT}/src/ovpn3cli/arghelpers.hpp
        ${OPENVPN3_LINUX_ROOT}/src/common/lookup.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/lookup.hpp
        ${OPENVPN3_LINUX_ROOT}/src/common/cmdargparser.cpp
        ${OPENVPN3_LINUX_ROOT}/src/configmgr/proxy-configmgr.hpp
        ${OPENVPN3_LINUX_ROOT}/src/configmgr/overrides.hpp
        ${OPENVPN3_LINUX_ROOT}/src/configmgr/overrides.cpp
        ${OPENVPN3_LINUX_ROOT}/src/sessionmgr/proxy-sessionmgr.hpp
        ${OPENVPN3_LINUX_ROOT}/src/dbus/requiresqueue-proxy.hpp
        ${OPENVPN3_LINUX_ROOT}/src/common/cmdargparser.hpp
        ${OPENVPN3_LINUX_ROOT}/src/common/cmdargparser.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/requiresqueue.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/timestamp.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/utils.cpp

        )

add_executable(openvpn3-service-netcfg
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/openvpn3-service-netcfg.cpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/core-tunbuilder.cpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg.hpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg-device.hpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg-exception.hpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg-options.hpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg-signals.hpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg-changeevent.hpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg-subscriptions.hpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg-subscriptions.cpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/dns/resolvconf-file.cpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/dns/resolver-settings.cpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/dns/settings-manager.cpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/core-tunbuilder.hpp
        ${OPENVPN3_LINUX_ROOT}/src/common/cmdargparser.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/lookup.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/requiresqueue.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/timestamp.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/utils.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/cmdargparser.cpp
        ${OPENVPN3_LINUX_ROOT}/src/log/core-dbus-logbase.hpp
        ${OPENVPN3_LINUX_ROOT}/src/log/dbus-log.hpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg-changeevent.cpp)

find_library(CAPNG cap-ng)
target_link_libraries(openvpn3-service-netcfg ${CAPNG})

add_executable(openvpn3-service-logger
        ${OPENVPN3_LINUX_ROOT}/src/log/openvpn3-service-logger.cpp
        ${OPENVPN3_LINUX_ROOT}/src/log/ansicolours.hpp
        ${OPENVPN3_LINUX_ROOT}/src/log/colourengine.hpp
        ${OPENVPN3_LINUX_ROOT}/src/log/dbus-log.hpp
        ${OPENVPN3_LINUX_ROOT}/src/log/log-helpers.hpp
        ${OPENVPN3_LINUX_ROOT}/src/log/logevent.hpp
        ${OPENVPN3_LINUX_ROOT}/src/log/logger.hpp
        ${OPENVPN3_LINUX_ROOT}/src/log/logwriter.hpp
        ${OPENVPN3_LINUX_ROOT}/src/log/service.hpp
        ${OPENVPN3_LINUX_ROOT}/src/common/timestamp.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/utils.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/cmdargparser.cpp
        )

add_executable(openvpn3-service-sessionmgr
        ${OPENVPN3_LINUX_ROOT}/src/sessionmgr/openvpn3-service-sessionmgr.cpp
        ${OPENVPN3_LINUX_ROOT}/src/sessionmgr/sessionmgr.hpp
        ${OPENVPN3_LINUX_ROOT}/src/client/statusevent.hpp
        ${OPENVPN3_LINUX_ROOT}/src/common/cmdargparser.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/lookup.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/requiresqueue.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/timestamp.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/utils.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/cmdargparser.cpp
        ${OPENVPN3_LINUX_ROOT}/src/log/dbus-log.hpp
        )

add_executable(cli-netcfg
        ${OPENVPN3_LINUX_ROOT}/src/tests/netcfg/cli.cpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg.hpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg-device.hpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg-exception.hpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg-options.hpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/proxy-netcfg.hpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/proxy-netcfg.cpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg-signals.hpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg-changeevent.hpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg-subscriptions.hpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg-subscriptions.cpp
        ${OPENVPN3_LINUX_ROOT}/src/netcfg/core-tunbuilder.hpp
        ${OPENVPN3_LINUX_ROOT}/src/common/lookup.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/requiresqueue.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/timestamp.cpp
        ${OPENVPN3_LINUX_ROOT}/src/common/utils.cpp
        ${OPENVPN3_LINUX_ROOT}/src/log/core-dbus-logbase.hpp
        ${OPENVPN3_LINUX_ROOT}/src/log/dbus-log.hpp
        )
