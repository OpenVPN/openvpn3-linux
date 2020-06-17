cmake_minimum_required(VERSION 3.10)
project(openvpn3_linux C CXX)
set(CMAKE_CXX_STANDARD 14)

add_executable(openvpn3-service-aws
    openvpn3-service-aws.cpp
    ${OPENVPN3_LINUX_ROOT}/src/dbus/signals.hpp
    ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg-changeevent.cpp
    ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg-changeevent.hpp
    ${OPENVPN3_LINUX_ROOT}/src/netcfg/proxy-netcfg.cpp
    ${OPENVPN3_LINUX_ROOT}/src/netcfg/netcfg-changetype.cpp
    ${OPENVPN3_LINUX_ROOT}/src/common/utils.cpp
    ${OPENVPN3_LINUX_ROOT}/src/common/utils.hpp
    ${OPENVPN3_LINUX_ROOT}/src/common/cmdargparser.cpp
    ${OPENVPN3_LINUX_ROOT}/src/common/timestamp.cpp
    ${OPENVPN3_LINUX_ROOT}/src/common/configfileparser.cpp
    ${OPENVPN3_LINUX_ROOT}/openvpn3-core/openvpn/aws/awsroute.hpp
    ${OPENVPN3_LINUX_ROOT}/src/netcfg/dco-keyconfig.proto
    ${CMAKE_BINARY_DIR}/src/netcfg/dco-keyconfig.pb.cc
)

target_compile_definitions(
    openvpn3-service-aws
    PRIVATE
    ${OPENVPN3_LINUX_CONFIG_DEFINES}
    -DHAVE_JSONCPP
    -DOVPN_TINYXML2_HAS_ERROR_NAME
    -DOVPN_TINYXML2_HAS_ERROR_STR
)

find_library(TINYXML2 tinyxml2)
target_link_libraries(
    openvpn3-service-aws
    ${TINYXML2}
    ${PROTOBUF_LIBRARIES}
)

set(CMAKE_MODULE_PATH "${OPENVPN3_LINUX_ROOT}/openvpn3-core/cmake;${CMAKE_MODULE_PATH}")
include(findcoredeps)
add_json_library(openvpn3-service-aws)
