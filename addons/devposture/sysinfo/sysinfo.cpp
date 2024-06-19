//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

#include "sysinfo.hpp"
#include <chrono>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <streambuf>
#include <stdexcept>
#include <sys/utsname.h>

namespace {

std::string search_os_release(const std::string &key, const std::string &contents)
{
    const std::regex REG{R"-((^|\n))-" + key + R"-(\s*=\s*"?([^"^\n]*)"?)-"};
    std::smatch m;

    if (std::regex_search(contents, m, REG))
        return m[2];

    return {};
}

} // end of anonymous namespace

namespace DevPosture {

SysInfo::SysInfo()
{
    using namespace std::string_literals;

    utsname un{};

    if (::uname(&un))
        throw std::runtime_error("Failed to retrieve OS information: "s + strerror(errno));

    uname = {un.sysname, un.machine, un.version, un.release};
    os_release.extra_version = un.release;

    std::ifstream oi("/etc/os-release");

    // It's fine, we have uname() fallbacks.
    if (!oi)
        return;

    const std::string contents((std::istreambuf_iterator<char>(oi)),
                               std::istreambuf_iterator<char>());

    os_release.version_id = search_os_release("VERSION_ID", contents);
    os_release.id = search_os_release("ID", contents);
    os_release.cpe = search_os_release("CPE_NAME", contents);

    std::string tentative_version = search_os_release("VERSION", contents);
    if (!tentative_version.empty())
        os_release.extra_version = tentative_version;
}

DateTime::DateTime()
{
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%F");
    date = ss.str();

    ss.str("");
    ss << std::put_time(std::localtime(&now_time_t), "%T");
    time = ss.str();

    ss.str("");
    ss << std::put_time(std::localtime(&now_time_t), "%z");
    timezone = ss.str();
}

} // namespace DevPosture
