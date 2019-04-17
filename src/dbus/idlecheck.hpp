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

#ifndef OPENVPN3_DBUS_IDLECHECK_HPP
#define OPENVPN3_DBUS_IDLECHECK_HPP

#include <iostream>
#include <memory>
#include <thread>

#include <openvpn/common/rc.hpp>

using namespace openvpn;

/**
 *   The IdleCheck class instruments a Glib2 based application
 *   with an automatic exit solution if the program it watches
 *   runs idle for too long.  How long is too long is defined
 *   by the parameter given to the constructor.
 *
 *   The main program will need to call the UpdateTimestamp()
 *   method whenever it does something which should reset the
 *   IdleCheck timer.
 *
 *   It also implements a reference counting.  If the reference
 *   counter is higher than 0, it will not exit regardless of the
 *   idle timer.
 *
 *   This class also setups up handling of SIGTERM and SIGINT
 *   signals, which will also ensure the program shuts down
 *   properly.
 */
class IdleCheck : public RC<thread_safe_refcount>
{
public:
    typedef RCPtr<IdleCheck> Ptr;

    /**
     *  IdleCheck constructor
     *
     * @param mainloop   Glib2 GMainLoop pointer, used to shutdown the app
     * @param idle_time  Defines how long the program can be idle before
     *                   automatically exiting
     */
    IdleCheck(GMainLoop *mainloop, std::chrono::duration<double> idle_time)
        : mainloop(mainloop),
          idle_time(idle_time),
          poll_time(idle_time),
          enabled(false),
          running(false),
          refcount(0)
    {
            UpdateTimestamp();
    }


    /**
     *   This resets the idle check timer.  This ensures
     *   the program will not exit until the next idle check,
     *   defined in the constructor.
     */
    void UpdateTimestamp()
    {
        last_operation = std::chrono::system_clock::now();
    }


    void SetPollTime(std::chrono::duration<double> pollt)
    {
        poll_time = pollt;
    }


    /**
     *   This enables the IdleCheck
     *
     *   The IdleCheck logic runs in a separate and
     *   independent thread
     */
    void Enable()
    {
        if (running)
        {
            return;
        }

        if (enabled)
        {
            return;
        }

        enabled = true;
        idle_checker.reset(new std::thread([self=Ptr(this)]()
                           {
                               self->_cb_idlechecker__loop();
                           }));
    }


    /**
     *   Disables the IdleCheck
     *
     *   This may also make the program exit if the
     *   reference counter is 0.
     */
    void Disable()
    {
        enabled = false;
        if (running)
        {
            exit_cv.notify_all();
        }
    }


    /**
     *   Increases the reference counter by 1
     */
    void RefCountInc()
    {
        refcount++;
    }


    /**
     *   Decreases the reference counter by 1
     */
    void RefCountDec()
    {
        refcount--;
    }


    /**
     *   Calls the join() method on the IdleCheck thread.
     *
     *   This is used during shutdown of the application,
     *   to cleanly shutdown all running threads
     */
    void Join()
    {
        idle_checker->join();
    }


    /**
     *   Main IdleCheck loop.
     *
     *   This is the main loop of the thread performing the
     *   IdleCheck logic
     */
    void _cb_idlechecker__loop()
    {
        running = true;
        while( enabled )
        {
            // FIXME: Consider to swap sleep_for() with
            // std::condition_variable::wait_for() which can
            // unlock on cv.notify_one() call.
            std::this_thread::sleep_for(poll_time);
            auto now = std::chrono::system_clock::now();
            if (0 == refcount && (last_operation + idle_time) < now)
            {
                // We timed out, start the main loop shutdown
#ifdef SHUTDOWN_NOTIF_PROCESS_NAME
            std::cout << SHUTDOWN_NOTIF_PROCESS_NAME
                      << " starting idle shutdown "
                      << "(pid: " << std::to_string(getpid()) << ")"
                      << std::endl;
#endif
                g_main_loop_quit(mainloop);
                enabled = false;
            }
        }
        running = false;
    }

private:
    GMainLoop *mainloop;
    std::chrono::duration<double> idle_time;
    std::chrono::duration<double> poll_time;
    bool enabled;
    bool running;
    uint16_t refcount;
    std::chrono::time_point<std::chrono::system_clock> last_operation;
    std::unique_ptr<std::thread> idle_checker;
};
#endif // OPENVPN3_DBUS_IDLECHECK_HPP
