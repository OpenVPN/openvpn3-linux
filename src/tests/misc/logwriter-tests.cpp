//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   logwriter-tests.cpp
 *
 * @brief  Simple independent unit test for the LogWriter interfaces.
 */

#include <iostream>
#include <unistd.h>

#include "config.h"
#include "log/ansicolours.hpp"
#include "log/logwriter.hpp"
#include "log/logwriters/implementations.hpp"

/**
 *  Tests the Write(std::string) method of the LogWriter implementation.
 *  This test will also toggle the timestamp off and on and write
 *  a log line without a timestamp.  This will not cause a difference in
 *  loggers who prepends the timestamp regardless of the input log data
 *  (like syslog).
 *
 * @param w  LogWriter instance to run tests on
 *
 */
void run_test_1(LogWriter &w)
{
    w.Write("**** run_test_1()");
    w.Write("Log line 1 with timestamp");
    w.Write("Log line 2 with timestamp");
    sleep(3);
    w.Write("Log line 3 - 3 seconds later");
    w.EnableTimestamp(false);
    w.Write("Log line 4 - without timestamp");
    w.EnableTimestamp(true);
    w.AddMeta("prepend", " [PREPENDED] ", true);
    w.PrependMeta("prepend");
    w.Write("Log line 5 - with timestamp again");
}


/**
 *  Tests the Write(LogGroup, LogCategory, std::string) method and will
 *  run a two loops ensuring all valid combinations of LogGroup and
 *  LogCategory are tested.
 *
 * @param w LogWriter instance to run tests on
 */
void run_test_2(LogWriter &w)
{
    w.Write("**** run_test_2()");
    for (int group = 1; group < LogGroupCount; group++)
    {
        for (int catg = 1; catg < 9; catg++)
        {
            if ((catg % 3) == 0)
            {
                w.AddMeta("prepend", "!! ---> Prepended data <--- ", true);
                w.PrependMeta("prepend", true);
            }
            w.Write((LogGroup)group,
                    (LogCategory)catg,
                    std::string("LogGroup/LogCategory test line: ") + std::to_string(group) + ":" + std::to_string(catg));
        }
    }
}


/**
 *  Tests the Write(LogEvent(...)) method and will
 *  run a two loops ensuring all valid combinations of LogGroup and
 *  LogCategory are tested.
 *
 * @param w LogWriter instance to run tests on
 */
void run_test_3(LogWriter &w)
{
    w.Write("**** run_test_3()");
    for (int group = 1; group < LogGroupCount; group++)
    {
        for (int catg = 1; catg < 9; catg++)
        {
            if ((catg % 3) == 0)
            {
                std::stringstream s;
                s << "!! ---> Prepended data <--- "
                  << ((catg % 2) == 0 ? "[with meta prepend] " : "");
                w.AddMeta("prepend", s.str(), true);
                w.PrependMeta("prepend", (catg % 2) == 0);
            }
            LogEvent ev((LogGroup)group,
                        (LogCategory)catg,
                        std::string("LogEvent() test line: ")
                            + std::to_string(group) + ":" + std::to_string(catg));
            w.AddMeta("meta",
                      "Meta data for test line:"
                          + std::to_string(group) + ":" + std::to_string(catg));
            w.Write(ev);
        }
    }
}


/**
 *  Tests turning on and off the meta line logging
 */
void run_test_4(LogWriter &w)
{
    w.Write("**** run_test_4() - every other line has a meta log line");
    for (int i = 1; i < 10; i++)
    {
        w.EnableLogMeta((i % 2) == 0);
        w.AddMeta("metaline_" + std::to_string(i),
                  "Meta data for line #" + std::to_string(i));
        w.Write("Log data for line #" + std::to_string(i));
    }
    w.EnableLogMeta(true);
}

int main(int argc, char **argv)
{
    // Simple text/plain log writer, logging to stdout
    std::cout << "Testing LogWriter" << std::endl
              << "----------------------------------------------------------"
              << std::endl;
    StreamLogWriter lfw(std::cout);
    run_test_1(lfw);
    run_test_2(lfw);
    run_test_3(lfw);
    run_test_4(lfw);
    std::cout << std::endl
              << std::endl;

    // Similar to the previous text/plain test, but uses the colours variant.
    // Still logging to stdout
    std::cout << "Testing ColourWriter, mode: by category" << std::endl
              << "----------------------------------------------------------"
              << std::endl;

    auto colours = new ANSIColours();
    ColourStreamWriter cfw(std::cout, colours);
    run_test_1(cfw);
    run_test_2(cfw);
    run_test_3(cfw);
    run_test_4(cfw);
    std::cout << std::endl
              << std::endl;

    std::cout << "Testing ColourWriter, mode: by group" << std::endl
              << "----------------------------------------------------------"
              << std::endl;
    colours->SetColourMode(ColourEngine::ColourMode::BY_GROUP);
    run_test_2(cfw);
    run_test_3(cfw);

    // Test the syslog implementation of LogWriter.  To validate these
    // log entries, the syslog log files needs to be evaluated.
    std::cout << "Testing SyslogWriter" << std::endl
              << "----------------------------------------------------------"
              << std::endl;
    SyslogWriter slw("logwriter:syslog", LOG_LOCAL1);
    run_test_1(slw);
    run_test_2(slw);
    run_test_3(slw);
    run_test_4(slw);
    std::cout << "Check the syslog for results" << std::endl
              << std::endl;

#if HAVE_SYSTEMD
    // Test the systemd-journald implementation of LogWriter.  To validate these
    // log entries, the journalctl needs to be evaluated.
    std::cout << "Testing JournaldWriter" << std::endl
              << "----------------------------------------------------------"
              << std::endl;
    JournaldWriter jlw;
    run_test_1(jlw);
    run_test_2(jlw);
    run_test_3(jlw);
    run_test_4(jlw);
    std::cout << "Check the systemd-journald for results" << std::endl
              << std::endl;
#endif // HAVE_SYSTEMD

    std::cout << "All tests done" << std::endl
              << std::endl;
    return 0;
}
