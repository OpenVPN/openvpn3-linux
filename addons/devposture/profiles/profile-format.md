Device Posture Enterprise Profile File Format
=============================================

A Device Posture Check (DPC) is triggered when both the VPN
server requests a DPC test to be executed _and_ that the local
VPN configuration profile being used is configured with an
Enterprise Profile.  This is configured using the
:code:`openvpn3 config-manage --enterprise-profile` option.

The Enterprise Profile defines which checks can be executed
on the host and the response format containing the result to
the server.


Base profile template
---------------------

These profiles uses the JSON format and there are four (4)
mandatory fields in the base template:

     {
        "name": "Description of the Enterprise Profile",
        "appcontrol_id": "APPCONTROL_ID",
        "ver": "1.0",
        "control_mapping": {
            // ...
        }
     }

The :code:`appcontrol_id` field is a low-level ID which the server
will request.  It is required to be a match between the `APPCONTROL_ID`
the server sends in its request and the *Enterprise Profile*  name the
VPN configuration profile is configured to use.

The :code:`ver` field is reported back to the server in the response
and the server will use this to know how to parse the result.

The :code:`control_mapping` is a dictionary providing a list of all
supported tests and which DPC modules to call to retrieve information.

Control Mapping
---------------

     "TEST-NAME": {
        "module": "D-BUS PATH TO DPC MODULE",
        "result_mapping": {
            // ...
        }
     }


The :code:`control_mapping` can contain more keys, one key per test.
When the VPN server requests a DPC test to be run, it will send this
test name to the VPN client in the :code:`dpc_request`.  There must be
a match between the test requested and the declared :code:`TEST-NAME`.

The :code:`module` must point at a DPC module.  This module is provided
by the `openvpn3-service-devposture` service, where each module has their
own unique D-Bus path.

The :code:`result_mapping` contains a mapping between the field names
used when responding back to the VPN servers DPC request and the "macro"
(or variable) name the test module provides.  The
`openvpn3-service-devposture` will replace these macro/variable names
with the corresponding value provided by the DPC module.

If a single DPC test wants to gather results from more DPC modules, each
module can setup using a JSON list:


     "TEST-NAME": [
        {
            "module": "D-BUS PATH TO DPC MODULE 1",
            "result_mapping": {
                // ...
            }
        },
        {
            "module": "D-BUS PATH TO DPC MODULE 2",
            "result_mapping": {
                // ...
            }
        }
     ]


Result Mapping
--------------

The possible data a DPC test can return depends on the macros/variables
a DPC module provides.  The standard `openvpn3-service-devposture` service
provides two internal modules.  The available macros/variables will be
defined in the following section.

A simple result mapping contains of a key value and the macro/variable.
It is also possible to nest results into a nested dictionary:

    "simple_mapping_1": "module_macro_1",
    "simple_mapping_2": "module_macro_2",
    "dictionary_map": {
        "simple_mapping3": "module_macro_3",
        "nested_map": {
            "simple_mapping4": "module_macro_4"
        }
    }

If the :code:`module_macro_1` provides the value `result:A`,
:code:`module_macro_2` provides `result:B`and so on, the DPC response
back to the server would result in this:

    "simple_mapping_1": "result:A",
    "simple_mapping_2": "result:B",
    "dictionary_map": {
        "simple_mapping3": "result:C",
        "nested_map": {
            "simple_mapping4": "result:D"
        }
    }


Example profiles
----------------

Two example profiles are shipped together with the
`openvpn3-service-devposture` service.  Please look at both the
`example1.json` and `example2.json` files for complete examples.
The first example defines two tests, :code:`host_info` and
:code:`client_time`.  The second profile provides a single test which
merges information from both the internal DPC modules.


Internal DPC modules
--------------------

* :code:`/net/openvpn/v3/devposture/modules/datetime`

  This module provides just a simple timestamp of when the DPC test was
  executed.  It provides three macros:

  - :code:`date`: The date, formatted as `YYYY-MM-DD`
  - :code:`time`: The time, formatted as `HH24:MM:SS`
  - :code:`timezone`: The timezone of the host, formatted as `±HHMM`

* :code:`/net/openvpn/v3/devposture/modules/platform`

  This module provides basic platform and operating system information.
  It provides these macros:

  - :code:`uname_sysname`: The same as running :code:`uname -s`, i.e. `Linux`
  - :code:`uname_machine`: The same as running :code:`uname -m`, i.e. `x86_64`
  - :code:`uname_version`: The same as running :code:`uname -r`, i.e. `6.8.3`
  - :code:`uname_release`: The same as running :code:`uname -v`, i.e. `#1 SMP Tue May 21 03:13:04 EDT 2024`

  The following macros are extracted from `/etc/os-release`, if present:
  - :code:`os_release_cpe`: Contains the content of :code:`CPE_NAME`
  - :code:`os_release_id`:  Contains the content of :code:`ID`
  - :code:`os_release_version_id`: Contains the content of :code:`VERSION_ID`
  - :code:`extra_version`: Contains the content of :code:`VERSION`
