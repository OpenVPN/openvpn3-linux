=================
openvpn3-secretmanager
=================

------------------------------------------------------
OpenVPN 3 secure credentials storage manager
------------------------------------------------------

:Manual section: 8
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3-secretmanager`` 
| ``openvpn3-secretmanager`` ``-h`` | ``--help``


DESCRIPTION
===========
The **openvpn3-secretmanager** utility is used to store and modify openvpn
passwords securely for use during autloaded user-auth sessions


OPTIONS
=======

-h, --help           Prints usage information and exits.



BACKGROUND
==========
By default there is no secure way to store auth-user login credentials
for autostart sessions - This utility adds an easy way to add new 
passwords to the system as well as provide a key for access.


USE
==========
The utility will ask for a password for storage, This password is your
user-auth password, after storage of the password the utility will return
a key. This key is used in place of a password in the .autoload to allow
libsecret to fetch the password from secure storage.



SEE ALSO
========

``openvpn3-autoload``\(1)


