=====================
openvpn3-configs-list
=====================

---------------------------------------------
OpenVPN 3 Linux - List Configuration Profiles
---------------------------------------------

:Manual section: 1
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3 configs-list`` ``-h`` | ``--help``


DESCRIPTION
===========
Lists all imported VPN configuration profiles, including configuration
names and D-Bus configuration paths.  It will only enlist configuration
profiles which a user has been granted access to or owns.

OPTIONS
=======

-h, --help               Print  usage and help details to the terminal

--count
        Instead of listing all configuration profiles, only report the
        number of configuration profiles found.  This will also consider
        any filters used.

-v, --verbose
        Provide more details about each configuration profile available.

--json
        Format the output as machine-readable JSON

--filter-config NAME-PREFIX
        Filter the list of configuration profiles to only consider
        profiles starting with the given prefix value.

--filter-tag TAG-VALUE
        Filter the list of configuration profiles to only consider
        profiles assigned with the given tag value.

--filter-owner OWNER
        Filter the list of configuration profiles to only consider
        profiles owned by the given user.  Only profiles accessible by
        the calling user will be listed.


SEE ALSO
========

``openvpn3``\(1)
``openvpn3-config-import``\(1)
