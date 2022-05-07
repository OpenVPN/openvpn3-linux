#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2018 - 2019  OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2018 - 2019  David Sommerseth <davids@openvpn.net>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Affero General Public License as
#  published by the Free Software Foundation, version 3 of the
#  License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Affero General Public License for more details.
#
#  You should have received a copy of the GNU Affero General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

##
# @file  SessionManager.py
#
# @brief  Provides a Python class to communicate with the OpenVPN 3
#         session manager service over D-Bus
#

import dbus
import time
from functools import wraps
from openvpn3.constants import StatusMajor, StatusMinor, SessionManagerEventType
from openvpn3.constants import ClientAttentionType, ClientAttentionGroup

##
#  The UserInputSlot object represents a single request for user input
#  by the backend VPN process
#
class UserInputSlot(object):
    ##
    #  Initialize a singe UserInputSlot object
    #
    #  @param session_intf   A session interface object to the related VPN session
    #  @param qtype          ClientAttentionType of the request
    #  @param qgroup         ClientAttentionGroup of the request
    #  @param qid            Unique request ID for this (type, group)
    #
    def __init__(self, session_intf, qtype, qgroup, qid):
        self.__session_interf = session_intf

        #  Retrieve the request slot
        qslot = self.__session_interf.UserInputQueueFetch(qtype.value,
                                                          qgroup.value,
                                                          qid)

        #  Sanity check - ensure we got what we requested
        if qtype != ClientAttentionType(qslot[0])       \
            or qgroup != ClientAttentionGroup(qslot[1]) \
            or qid != qslot[2]:
            raise RuntimeError('Mismatch in the UserInput queue slot')

        # Parse and save the request information
        self.__qtype = qslot[0]
        self.__qgroup = qslot[1]
        self.__qid = qslot[2]
        self.__varname = qslot[3]
        self.__label = qslot[4]
        self.__mask = qslot[5]


    def __repr__(self):
        return '<UserInputSlot queue_type=%s, queue_group=%s, queue_id=%s, varname="%s", label="%s", masked_input=%s>' % (
            str(ClientAttentionType(self.__qtype)),
            str(ClientAttentionGroup(self.__qgroup)),
            str(self.__qid), self.__varname, self.__label,
            self.__mask and 'True' or 'False')


    def GetTypeGroup(self):
        return (ClientAttentionType(self.__qtype),
                ClientAttentionGroup(self.__qgroup))

    def GetVariableName(self):
        return self.__varname

    def GetLabel(self):
        return self.__label

    def GetInputMask(self):
        return self.__mask

    def ProvideInput(self, value):
        self.__session_interf.UserInputProvide(self.__qtype,
                                               self.__qgroup,
                                               self.__qid,
                                               value)



##
#  The Session object represents a single OpenVPN VPN session as
#  presented via the OpenVPN 3 session manager D-Bus service.
class Session(object):
    ##
    #  Initialize the Session object.  It requires a D-Bus connection
    #  object and the D-Bus object path to the VPN session
    #
    #  @param dbuscon  D-Bus connection object
    #  @param objpath  D-Bus object path to the VPN session
    #
    def __init__(self, dbuscon, objpath):
        self.__dbuscon = dbuscon

        # Retrieve access to the session object
        self.__session = self.__dbuscon.get_object('net.openvpn.v3.sessions',
                                                   objpath)

        # Retrive access to the session interface in the object
        self.__session_intf = dbus.Interface(self.__session,
                                             dbus_interface="net.openvpn.v3.sessions")

        # Retrive access to the property interface in the object
        self.__prop_intf = dbus.Interface(self.__session,
                                          dbus_interface="org.freedesktop.DBus.Properties")

        self.__session_path = objpath
        self.__log_callback = None
        self.__status_callback = None
        self.__deleted = False


    def __del__(self):
        try:
            if self.__log_callback is not None:
                self.LogCallback(None)
            if self.__status_callback is not None:
                self.StatusChangeCallback(None)
        except ImportError:
            # This happens if Python is already shutting down
            # no chance to properly clean up at this point.
            pass


    ##
    #  Internal decorator, checkes whether the object has been deleted or not.
    #  If the object has been deleted, throw an exception instead.
    #
    #  For details, lookup how Python decorators work
    #
    def __delete_check(func):
        @wraps(func)
        def __delete_checker(self, *args, **kwargs):
            if self.__deleted is True:
                raise RuntimeError("This session object is unavailable")
            return func(self, *args, **kwargs)
        return __delete_checker

    ##
    #  Returns the D-Bus configuration object path
    #
    #  @return String containing the D-Bus object path
    #
    @__delete_check
    def GetPath(self):
        return dbus.ObjectPath(self.__session_path)


    ##
    #  Sets a specific property in the VPN session object
    #
    #  @param propname   String containing the property name to modify
    #  @param propvalue  The new value the property should have. The data
    #                    type ov the value must match the data type of the
    #                    property in the D-Bus object
    #
    @__delete_check
    def SetProperty(self, propname, propvalue):
        self.__prop_intf.Set('net.openvpn.v3.sessions',
                             propname, propvalue)


    ##
    #  Retrieve the value of a property in a VPN session object
    #
    #   @param propname  String containing the property name to query
    #
    @__delete_check
    def GetProperty(self, propname):
        return self.__prop_intf.Get('net.openvpn.v3.sessions', propname)


    ##
    #  Checks if the VPN backend process is ready to start connecting
    #
    @__delete_check
    def Ready(self):
        self.__session_intf.Ready()


    ##
    #  Tells the VPN backend process to start the connection
    #
    @__delete_check
    def Connect(self):
        self.__session_intf.Connect()


    ##
    #  Tells the VPN backend process to pause the connection
    #
    #  @param reason  String containing a reason why the session was paused.
    #                 This is used primarily for logging purposes and defaults
    #                 to 'User request'
    #
    @__delete_check
    def Pause(self, reason='User request'):
        self.__session_intf.Pause(reason)


    ##
    #  Tells the VPN backend process to resume a paused connection
    #
    @__delete_check
    def Resume(self):
        self.__session_intf.Resume()


    ##
    #  Tells the VPN backend process to restart the connection
    #
    @__delete_check
    def Restart(self):
        self.__session_intf.Restart()


    ##
    #  Tells the VPN backend to disconnect and shutdown the connection
    #
    @__delete_check
    def Disconnect(self):
        self.__deleted = True
        self.__session_intf.Disconnect()


    ##
    #  Retrive the session status
    #
    #  @return  Returns a type of (StatusMajor, StatusMinor, Status message)
    #           The Status message is a plain string.
    @__delete_check
    def GetStatus(self):
        status = self.__prop_intf.Get('net.openvpn.v3.sessions',
                                      'status')
        return {"major": StatusMajor(status[0]),
                "minor": StatusMinor(status[1]),
                "message": status[2]}


    ##
    #  Retrieve session statistics
    #
    #  @return Returns a formatted string containing the statistics
    #
    @__delete_check
    def GetStatistics(self):
        return self.__prop_intf.Get('net.openvpn.v3.sessions',
                                        'statistics')

    ##
    #  Retrieve session statistics as a pre-formatted string
    #
    #  @param prefix      Start the result with the provides string
    #                     Defaults to: 'Connection statistics:\n'
    #  @param format_str  Format string to use.  It will always be a string
    #                     and an integer, in that order.
    #                     Defaults to: '    %25s: %i\n'
    #  @return Returns a formatted string containing the statistics
    #
    @__delete_check
    def GetFormattedStatistics(self, prefix='Connection statistics:\n', format_str='    %25s: %i\n'):
        statistics = self.GetStatistics()
        ret = ""
        if len(statistics) > 0:
            ret += prefix
            for (key, val) in statistics.items():
                ret += format_str % (key, val)

        return ret


    ##
    #  Subscribes to the Log signals for this session, enables the log
    #  forwarding and register a callback function which called on each event
    #
    #  The callback function needs to accept 3 arguments:
    #     (integer) LogGroup, (integer) LogCategory, (string) message
    #
    #
    def LogCallback(self, cbfnc):
        if cbfnc is not None:
            self.__log_callback = cbfnc
            self.__dbuscon.add_signal_receiver(cbfnc,
                                               signal_name='Log',
                                               dbus_interface='net.openvpn.v3.backends',
                                               bus_name='net.openvpn.v3.log',
                                               path=self.__session_path)
            self.__session_intf.LogForward(True)
        else:
            try:
                self.__session_intf.LogForward(False)
            except dbus.exceptions.DBusException:
                # If this fails, the session is typically already removed
                pass
            self.__dbuscon.remove_signal_receiver(self.__log_callback, 'Log')
            self.__log_callback = None


    ##
    #  Subscribes to the StatusChange signals for this session and register
    #  a callback function which is being called on each event
    #
    #  The callback function needs to accept 3 arguments:
    #     (integer) StatusMajor, (integer) StatusMinor, (string) message
    #
    def StatusChangeCallback(self, cbfnc):
        if cbfnc is not None:
            self.__status_callback = cbfnc
            self.__dbuscon.add_signal_receiver(cbfnc,
                                               signal_name='StatusChange',
                                               dbus_interface='net.openvpn.v3.backends',
                                               bus_name='net.openvpn.v3.log',
                                               path=self.__session_path)
        else:
            self.__dbuscon.remove_signal_receiver(self.__status_callback,
                                                  'StatusChange')
            self.__status_callback = None



    ##
    #  Subscribes to the AttentionRequired signals for this session and register
    #  a callback function which is being called on each event
    #
    # The callback function needs to accept 3 arguments:
    #     (integer) AttentionType, (integer) AttentionGroup, (string) message
    #
    def AttentionRequiredCallback(self, cbfnc):
        self.__session_intf.connect_to_signal('AttentionRequired', cbfnc)


    ##
    #  Queries the VPN backend if the user needs to be queried for information
    #
    #  @return Returns a list of tuples of Queue types and groups which needs
    #          to be satisfied
    #
    @__delete_check
    def UserInputQueueGetTypeGroup(self):
        ret = []
        for (qt, qg) in self.__session_intf.UserInputQueueGetTypeGroup():
            ret.append((ClientAttentionType(qt), ClientAttentionGroup(qg)))
        return ret


    ##
    #  Queries the VPN backend for query slots needing to be satisifed within
    #  a queue type and group.
    #
    #  @param  qtype   Queue type to check
    #  @param  qgroup  Queue group to check
    #
    #  @returns a list of unique ID references to slots needing to be satisfied
    #
    @__delete_check
    def UserInputQueueCheck(self, qtype, qgroup):
        return self.__session_intf.UserInputQueueCheck(qtype.value,
                                                       qgroup.value)


    ##
    #  Retrieve information about a specific user input slot which needs to be
    #  satisfied.  This provides information what to query for and how to
    #  process the data
    #
    #  @param  qtype   Queue type of the user input slot
    #  @param  qgroup  Queue group of the user input slot
    #  @param  qid     Queue ID of the user inout slot to retrieve
    #
    #  @return Returns a list containing all the details needing to be
    #          satisfied
    #
    @__delete_check
    def UserInputQueueFetch(self, qtype, qgroup, qid):
        return UserInputSlot(self.__session_intf, qtype, qgroup, qid)


    ##
    #  Simpler Python approach to retrieve all required user inputs.
    #  This method will return a list of UserInputSlot objects which can
    #  be used to extract information to present to the user and provide the
    #  user input back to the backend VPN process
    #
    #  @return Returns a list of UserInputSlot objects which must be processed
    #
    @__delete_check
    def FetchUserInputSlots(self):
        ret = []
        for (qt, qg) in self.UserInputQueueGetTypeGroup():
            for qid in self.UserInputQueueCheck(qt, qg):
                ret.append(UserInputSlot(self.__session_intf, qt, qg, qid))
        return ret


    ##
    #  Retrieve the Data Channel Offset (DCO) setting for a running VPN session
    #
    #  @return Returns True if the session DCO kernel acceleration has
    #          been enabled
    #
    @__delete_check
    def GetDCO(self):
        return self.__prop_intf.Get('net.openvpn.v3.sessions', 'dco')


    ##
    #  Change the Data Channel Offset (DCO) setting for a running VPN session.
    #  This can only be modified *BEFORE* the Connect() method has been called.
    #
    @__delete_check
    def SetDCO(self, dco):
        self.__prop_intf.Set('net.openvpn.v3.sessions', 'dco', dco)



##
#  This class will be instantiated on each SessionManagerEvent D-Bus
#  signal and sent to the callback function.  It will contain
#  all information about the event
#
class SessionManagerEvent(object):
    def __init__(self, path, type, owner):
        self.__path = path
        self.__type = SessionManagerEventType(type)
        self.__owner = owner

    def GetPath(self):
        return self.__path

    def GetType(self):
        return self.__type

    def GetOwner(self):
        return self.__owner

    def __repr__(self):
        return '<SessionManagerEvent type="%s", path="%s", owner_uid=%i>' % (
            self.__type.name, str(self.__path), self.__owner)

    def __str__(self):
        return '[SESSION] %s: %s (owner: %i)' % (
            self.__type.name, str(self.__path), self.__owner)

    def __eq__(self, other):
        if isinstance(other, SessionManagerEventType):
            return other == self.__type
        elif isinstance(other, str) or isinstance(other, dbus.ObjectPath):
            return str(other) == str(self.__path)
        return False


##
#  The SessionManager object provides access to the main object in
#  the session manager D-Bus service.  This is primarily used to create
#  new VPN tunnel sessions, but can also be used to retrieve specific objects
#  when the session D-Bus object path is known.
#
class SessionManager(object):
    ##
    #  Initialize the SessionManager object
    #
    #  @param  dbuscon   D-Bus connection object to use
    #
    def __init__(self, dbuscon):
        self.__dbuscon = dbuscon

        # Retrieve the main sessoin manager object
        self.__manager_object = dbuscon.get_object('net.openvpn.v3.sessions',
                                                   '/net/openvpn/v3/sessions')

        # Retireve access to the session interface in the object
        self.__manager_intf = dbus.Interface(self.__manager_object,
                                             dbus_interface='net.openvpn.v3.sessions')

        # Retrive access to the property interface in the object
        self.__prop_intf = dbus.Interface(self.__manager_object,
                                          dbus_interface="org.freedesktop.DBus.Properties")

        # Setup a simple access to the Peer interface, for Ping()
        self.__peer_intf = dbus.Interface(self.__manager_object,
                                          dbus_interface='org.freedesktop.DBus.Peer')


    def Introspect(self):
        intf = dbus.Interface(self.__manager_object,
                              dbus_interface='org.freedesktop.DBus.Introspectable')
        return intf.Introspect()


    def GetObjectPath(self):
        return self.__manager_object.object_path


    ##
    #  Create a new VPN session
    #
    #  @param cfgobj      openvpn3.Configuration object to use for this new
    #                     session
    #
    #  @return Returns a Session object of the imported configuration
    #
    def NewTunnel(self, cfgobj):
        self.__ping()
        path = self.__manager_intf.NewTunnel(cfgobj.GetPath())
        return Session(self.__dbuscon, path)


    ##
    #  Retrieve a single Session object for a specific configuration path
    #
    #  @param objpath   D-Bus object path to the VPN session to retrieve
    #
    #  @return Returns a Session object of the requested VPN session
    #
    def Retrieve(self, objpath):
        self.__ping()
        return Session(self.__dbuscon, objpath)


    ##
    #  Retrieve a list of all available VPN sessions in the
    #  session manager
    #
    #  @return Returns a list of Session object, one for each session
    #
    def FetchAvailableSessions(self):
        ret = []
        self.__ping()
        for s in self.__manager_intf.FetchAvailableSessions():
            ret.append(Session(self.__dbuscon, s))
        return ret


    ##
    #  Looks up a configuration name to find available session objects
    #  started with the given configuration name
    #
    #  @return Returns a list of D-Bus path objects matching the search
    #          criteria
    def LookupConfigName(self, cfgname):
        self.__ping()
        return self.__manager_intf.LookupConfigName(cfgname)


    ##
    #  Transfer the ownership of a given session path to a new user (UID)
    #
    #  @param sesspath  D-Bus object path to the session
    #  @param new_uid   UID of the new owner of this session object
    #
    def TransferOwnership(self, sesspath, new_uid):
        self.__manager_intf.TransferOwnership(dbus.ObjectPath(sesspath),
                                              dbus.UInt32(new_uid))


    ##
    #  Subscribes to the SessionManagerEvent signals from the session manager
    #  and register a callback function which is being called on each event
    #
    #  The callback function needs to accept 1 arguments:
    #    a SessionManagerEvent object
    #
    def SessionManagerCallback(self, cbfnc):
        self.__sessmgr_callback_func = cbfnc
        self.__manager_intf.connect_to_signal('SessionManagerEvent', self.__sessmgr_event_cb_wrapper)


    ##
    #  Private method, which sends a Ping() call to the main D-Bus
    #  interface for the service.  This is used to wake-up the service
    #  if it isn't running yet.
    #
    def __ping(self):
        delay = 0.5
        attempts = 10

        while attempts > 0:
            try:
                self.__peer_intf.Ping()
                self.__prop_intf.Get('net.openvpn.v3.sessions', 'version')
                return
            except dbus.exceptions.DBusException as excp:
                err = str(excp)
                if err.find("org.freedesktop.DBus.Error.AccessDenied:") > 0:
                    raise RuntimeError("Access denied to the Session Manager (ping)")
                time.sleep(delay)
                delay *= 1.33
            attempts -= 1
        raise RuntimeError("Could not establish contact with the Session Manager")


    ##
    #  Internal wrapper method to call the SessionManagerEvent signal handler
    #  with a SessionManagerEvent Python object
    #
    def __sessmgr_event_cb_wrapper(self, path, evtype, owner):
        self.__sessmgr_callback_func(SessionManagerEvent(path, evtype, owner))
