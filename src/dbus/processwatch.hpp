//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017 - 2022  OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017 - 2022  David Sommerseth <davids@openvpn.net>
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

#include <mutex>
#include <condition_variable>

#include <openvpn/common/rc.hpp>

class ProcessSignalWatcher : public DBusSignalSubscription,
                             public RC<thread_unsafe_refcount>
{
public:
    typedef RCPtr<ProcessSignalWatcher> Ptr;

    ProcessSignalWatcher(GDBusConnection *dbuscon)
        : DBusSignalSubscription(dbuscon, "", "", "/", "ProcessChange"),
          waitfor_inuse(false)
    {
    }


    StatusMinor WaitForProcess(std::string interface, std::string procname, uint8_t timeout = 10)
    {
        return start_wait(interface, procname, 0, timeout);
    }


    StatusMinor WaitForProcess(std::string interface, pid_t pid, uint8_t timeout = 10)
    {
        return start_wait(interface, "", pid, timeout);
    }


    void callback_signal_handler(GDBusConnection *connection,
                                 const std::string sender_name,
                                 const std::string object_path,
                                 const std::string interface_name,
                                 const std::string signal_name,
                                 GVariant *parameters)
    {
        if (signal_name != "ProcessChange")
        {
            return;
        }

        StatusMinor status;
        gchar *procname_p = NULL;
        pid_t pid;
        g_variant_get(parameters, "(usu)", &status, &procname_p, &pid);
        std::string procname(procname_p);

        if (!waitfor_process_name.empty() && (procname != waitfor_process_name))
        {
            // Not the process name we're waiting for
            return;
        }

        if (waitfor_pid > 0 && (pid != waitfor_pid))
        {
            // Not the process name we're waiting for
            return;
        }

        waitfor_result = status;
        waitfor_signaled = true;
        std::unique_lock<std::mutex> lock(waitfor_mtx);
        lock.unlock();
        waitfor_condvar.notify_one();
    }


private:
    std::mutex waitfor_mtx;
    std::condition_variable waitfor_condvar;
    bool waitfor_inuse;
    StatusMinor waitfor_result;
    std::string waitfor_process_name;
    pid_t waitfor_pid;
    bool waitfor_signaled;

    StatusMinor start_wait(std::string interface, std::string process_name, pid_t pid, uint8_t timeout)
    {
        if ((process_name.empty() && pid == 0) || (!process_name.empty() && pid > 0))
        {
            THROW_DBUSEXCEPTION("ProcessSignalWatcher", "Process name *OR* PID must be set");
        }

        if (waitfor_inuse)
        {
            THROW_DBUSEXCEPTION("ProcessSignalWatcher", "A WaitForProcess() call is already running.");
        }
        waitfor_inuse = true;

        // Subscribe for StatusChange signals on the provided interface
        waitfor_process_name = process_name;
        waitfor_result = StatusMinor::UNSET;
        waitfor_signaled = false;
        waitfor_pid = pid;

        guint signal_id = g_dbus_connection_signal_subscribe(GetConnection(), NULL, interface.c_str(),
                                                             "ProcessChange", "/",
                                                             NULL,
                                                             G_DBUS_SIGNAL_FLAGS_NONE,
                                                             dbusobject_callback_signal_handler,
                                                             this,
                                                             NULL);
        std::unique_lock<std::mutex> lock(waitfor_mtx);
        waitfor_condvar.wait_for(lock, std::chrono::seconds(timeout),
                                 [this]{return true == waitfor_signaled;});

        g_dbus_connection_signal_unsubscribe(GetConnection(), signal_id);

        StatusMinor ret = waitfor_result;  // To avoid another caller to clobbering our result
        waitfor_inuse = false;
        return ret;

    }

};



class ProcessSignalProducer : public DBusSignalProducer,
                              public RC<thread_unsafe_refcount>
{
public:
    typedef RCPtr<ProcessSignalProducer> Ptr;

    ProcessSignalProducer(GDBusConnection *dbuscon, std::string interface, std::string procname)
        : DBusSignalProducer(dbuscon, "", interface, "/"),
          processname(procname)
    {
    }


    ProcessSignalProducer(GDBusConnection *dbuscon, std::string interface, std::string objpath,
                          std::string procname)
        : DBusSignalProducer(dbuscon, "", interface, objpath),
          processname(procname)
    {
    }


    void ProcessChange(const StatusMinor status)
    {
        GVariant *params = g_variant_new("(usu)", (guint) status, processname.c_str(), getpid());
        Send("ProcessChange", params);
        if (StatusMinor::PROC_STOPPED == status)
        {
            // Give a little grace period when a process is shutting down,
            // to ensure the signal is sent before the process stops
            usleep(300);
        }
    }


private:
    std::string processname;
};
