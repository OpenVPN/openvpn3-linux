#!/usr/bin/python3
#
#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2025-  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2025-  David Sommerseth <davids@openvpn.net>
#
#
#  Simple test script importing a configuration profile with
#  various characters to expected to be filtered out.

import dbus
import sys
import openvpn3
from pprint import pprint

# Get a connection to the D-Bus bus
bus = None
if len(sys.argv) > 1 and '--use-session-bus' == sys.argv[1]:
    bus = dbus.SessionBus()
else:
    bus = dbus.SystemBus()

# Connect to the Configuration Manager
cfgmgr = openvpn3.ConfigurationManager(bus)

#
# Here is our super simple configuration file (not valid by the way, lacks --ca)
# all external files must be embedded before it can be imported, otherwise the
# backend client will not be able to read them.
#
config_to_import = """
remote vpn1.example.org
port 30001
proto udp
client-cert-not-required
auth-user-pass
new-line-test start extra arg\nafter-new-line argument\nblabla
ctrlchars_red change-to-colour-\x1b[31mred
"""

# Import some test configurations
errors = False

cfgname_red = 'client_\x1b[31mred.conf'
cfg_colour = cfgmgr.Import(cfgname_red,
                           config_to_import,  # config_str
                           False,             # single_use
                           False,             # persistent
                           'testcase')        # system tag
cfg_colour.AddTag('test_ctrlchar_colour')
cfg_colour.AddTag('tag_\x1b[31mred')
print("Configuration path (ASCII colour): " + cfg_colour.GetPath())
if 'client_[31mred.conf' != cfg_colour.GetConfigName():
    print("!! FAILED TEST (config name): ",)
    pprint(cfg_colour.GetConfigName())
    print("\n")
    errors = True

for tag in cfg_colour.GetTags():
    if 'tag_\x1b[31mred' == tag:
        print("!! FAILED TEST (tag with red colour): ")
        pprint(tag)
        print("\n")
        errors = True

cfg_colour.Remove()


cfgname_nl = "client.conf'\n\rWed Apr 16 12:34:56 2025 You are likely to be eaten by a grue"
cfg_nl = cfgmgr.Import(cfgname_nl,
                       config_to_import,  # config_str
                       False,             # single_use
                       False,             # persistent
                       'testcase')        # system tag          
cfg_nl.AddTag('test_ctrlchar_nl')
cfg_nl.AddTag('test_ctrlchar_\rwith\n_nl')
print("Configuration path (newline): " + cfg_nl.GetPath())
if cfgname_nl == cfg_nl.GetConfigName():
    print("!! FAILED TEST (config name, newline): ")
    pprint(cfg_nl.GetConfigName())
    print("\n")
    errors = True


for tag in cfg_nl.GetTags():
    if 'test_ctrlchar_\rwith\n_nl' == tag:
        print("!! FAILED TEST (tag with red colour): ")
        pprint(tag)
        print("\n")
        errors = True

cfg_plain = cfg_nl.Fetch()
tests = {
    'ctrlchars_red': ['change-to-colour-[31mred'],
    'new-line-test': ['start', 'extra', 'arg'],
    'after-new-line': ['argument'],
    'port': ['30001']
}

err_plain = False
for l in cfg_plain.split('\n'):
    line = l.split(' ')
    if len(line) > 1:
        if line[0] in tests and tests[line[0]] != line[1:]:
            print("!! FAILED TEST (config content, plain): %s" % line[0])
            pprint(line[1:])
            print("Expected: %s" % str(tests[line[0]]))
            print("\n")
            err_plain = True

if not err_plain:
    print("Fetch() call succeeded")


    err_json = False
for k,v in cfg_nl.FetchJSON().items():
    if k in tests and tests[k] != v:
            print("!! FAILED TEST (config content, plain): %s" % k)
            pprint(v)
            print("Expected: %s" % str(tests[k]))
            print("\n")
            err_json = True
cfg_nl.Remove()

if not err_json:
    print("FetchJSON() call succeeded")

if err_plain or err_json or errors:
    sys.exit(1)
