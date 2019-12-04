//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017 - 2019  OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017 - 2019  David Sommerseth <davids@openvpn.net>
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

#pragma once

#include <iostream>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <glib-unix.h>

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
        : signal_caught(false),
          mainloop(mainloop),
          idle_time(idle_time),
          enabled(false),
          running(false),
          refcount(0)
    {
            g_unix_signal_add(SIGINT, _cb__idlechecker_sighandler,
                              (void *) this);
            g_unix_signal_add(SIGTERM, _cb__idlechecker_sighandler,
                              (void *) this);

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
     *   Checks if the IdleCheck is enabled or not
     *
     * @return  Returns true if IdleCheck is enabled
     */
    bool GetEnabled()
    {
        return enabled;
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
        while (enabled)
        {
            // Wait until the timer completes or we get
            // notification triggered by SIGINT/SIGTERM signals
            std::unique_lock<std::mutex> exit_lock(exit_cv_mutex);
            exit_cv.wait_for(exit_lock, idle_time,
                             [self=Ptr(this)]
                              {
                                    // Do not re-trigger the timer
                                    // if we have received a signal
                                    return self->signal_caught;
                              });

            if (idle_time + last_operation < std::chrono::system_clock::now()
                || signal_caught)
            {
#ifdef SHUTDOWN_NOTIF_PROCESS_NAME
                if (0 == refcount && !signal_caught)
                {
                    // We timed out, start the main loop shutdown
                    std::cout << SHUTDOWN_NOTIF_PROCESS_NAME
                              << " starting idle shutdown "
                              << "(pid: " << std::to_string(getpid()) << ")"
                              << std::endl;
                }
#endif

                // If receiving signals, we exit regardless
                // of the reference counting state
                if (0 == refcount || signal_caught)
                {
                    g_main_loop_quit(mainloop);
                    enabled = false;
                }
            }
        }
        running = false;
    }


    /**
     *  IdleCheck signal handler callback function.
     *
     *  This is called whenever the subscribed signals in
     *  the constructor.  Typically SIGINT and SIGTERM.
     *
     * @param  data  Carries a pointer to this IdleCheck object
     *
     * @return See @GSourceFunc() declaration in glib2.  We return
     *         G_SOURCE_CONTINUE as we do not want to remove/disable the
     *         signal processing.
     */
    static int _cb__idlechecker_sighandler(void *data)
    {
        IdleCheck *self = (IdleCheck *)data;
        self->signal_caught = true;
        self->Disable();
        return G_SOURCE_CONTINUE;
    }


    // We make this public for simplicity.  Otherwise it would require
    // both a setter and getter method as it is used by both the
    // signal handler below and in the wait_for() lambda in the main
    // IdleCheck loop.
    bool signal_caught;  /**< Indicates if a signal has been received */


private:
    GMainLoop *mainloop;
    std::chrono::duration<double> idle_time;
    bool enabled;
    bool running;
    uint16_t refcount;
    std::chrono::time_point<std::chrono::system_clock> last_operation;
    std::unique_ptr<std::thread> idle_checker;
    std::mutex exit_cv_mutex;
    std::condition_variable exit_cv;
};
