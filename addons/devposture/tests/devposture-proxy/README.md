Device Posture Check Test Program (devposture-proxy)
====================================================

This program is used to interact with the Device Posture Check service
provided by the ``openvpn3-service-devposture`` service.

The purpose of this service is to collect certain information from the
host it runs on request by a VPN server and provide that result back to
the VPN server.  This is done via "Device Posture Check Requests", aka
:code:`dpc_-_request` objects.  This is a JSON object sent to the VPN client
by the server and follows a standardised `dpc-request` format.  The locally
running `openvpn3-service-devposture` service will parse these requests and
provide a JSON :code:`dpc_response` response back.  To test this service
locally without a server setup, the `devposture-proxy` tool will generate
the `dpc_request` object and present the result from the service.

Two example profiles are shipped together with the basic
`openvpn3-service-devposture` installation; :code:`example1.json` and
:code:`example2.json`.  The filename stem (filename without extension),
declares what is called the *Device Posture Enterprise Profile*.

Examples
--------
The `./devposture-proxy` tool must be run as :code:`root`.  The default
configuration expects only the :code:`openvpn` user to call the
`openvpn3-service-devposture` service.  To achieve this, this test tool
will drop all privileges to become :code:`openvpn` before establishing
a connection to the device posture check service.

### List supported DPC checker modules

     # ./devposture --list-modules

### Run the defined tests in the :code:`example1` Enterprise Profile

     # ./devposture-proxy --enterprise-profile example1 \
                          --test host_info --test client_time

The tests `host_info` and `client_time` are defined in the
:code:`example1.json` file.

### Run the defined tests in the :code:`example1` Enterprise Profile

     # ./devposture-proxy --enterprise-profile example2 \
                          --test merged-results

This profile uses a variant of the first example, where all the tests
from both test modules

### Run a specific DPC protocol check

The examples provides defines two different response protocols;
:code:`dpc-example1` and :code:`dpc-example2`.  To run just the `host_info`
check as defined in the `dpc-example1` protocol:

     # ./devposture-proxy --protocol dpc-example1 \
                          --test host_info
