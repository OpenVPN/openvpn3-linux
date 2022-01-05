OpenVPN 3 D-Bus API: Configuration manager
==========================================

The configuration manager is responsible for storing VPN
profiles/configuration. There are two types of profiles, persistent
and non-persistent. The persistent profiles are saved to disk. The
non-persistent are only available in memory, so when the configuration
manager stops running the configuration will not be available.


D-Bus destination: `net.openvpn.v3.configuration` - Object path: `/net/openvpn/v3/configuration`
------------------------------------------------------------------------------------------------

```
node /net/openvpn/v3/configuration {
  interface net.openvpn.v3.configuration {
    methods:
      Import(in  s name,
             in  s config_str,
             in  b single_use,
             in  b persistent,
             out o config_path);
      FetchAvailableConfigs(out ao paths);
      LookupConfigName(in  s config_name,
                       out ao config_paths);
      TransferOwnership(in  o path,
                        in  u new_owner_uid);
    signals:
      Log(u group,
          u level,
          s message);
    properties:
          readonly s version;
  };
};
```

### Method: `net.openvpn.v3.configuration.Import`

This method imports a configuration profile. The configuration must be
represented as a string blob containing everything.

#### Arguments

| Direction | Name        | Type        | Description                                                           |
|-----------|-------------|-------------|-----------------------------------------------------------------------|
| In        | name        | string      | User friendly name of the profile. To be used in user front-ends      |
| In        | config_str  | string      | Content of config file. All files must be embedded inline             |
| In        | single_use  | boolean     | If set to true, it will be removed from memory on first use           |
| In        | persistent  | boolean     | If set to true, the configuration will be saved to disk               |
| Out       | config_path | object path | A unique D-Bus object path for the imported VPN configuration profile |

### Method: `net.openvpn.v3.configuration.FetchAvailableConfigs`

This method will return an array of object paths to configuration objects the
caller is granted access to.

#### Arguments
| Direction | Name        | Type         | Description                                                           |
|-----------|-------------|--------------|-----------------------------------------------------------------------|
| Out       | paths       | object paths | An array of object paths to accessbile configuration objects          |


### Method: `net.openvpn.v3.configuration.LookupConfigName`

This method will return an array of object paths to configuration objects the
caller is granted access with the configuration name provided to the method.

#### Arguments
| Direction | Name         | Type         | Description                                                           |
|-----------|--------------|--------------|-----------------------------------------------------------------------|
| In        | config_name  | string       | String containing the configuration name for the configuration path lookup      |
| Out       | config_paths | object paths | An array of object paths to accessible configuration objects          |


### Method: `net.openvpn3.v3.configuration.TransferOwnership`

This method transfers the ownership of a configuration profile  to the given
UID value.  This feature is by design restricted to the root account only and
is only expected to be used by `openvpn3-autoload` and similar tools.

This method is also placed in the main configuration manager object and not the
configuration object itself by design, to emphasize this being a special case
feature.  This also makes it easier to control this feature in the D-Bus policy
in addition to the hard-coded restriction in the configuration manager code.

#### Arguments
| Direction | Name          | Type         | Description                                                  |
|-----------|---------------|--------------|--------------------------------------------------------------|
| In        | path          | object path  | Configuration object path where to modify the owner property |
| In        | new_owner_uid | unsigned int | UID value of the new owner of the configuration profile      |


### Signal: `net.openvpn.v3.configuration.Log`

Whenever the configuration manager want to log something, it issues a
Log signal which carries a log group, log verbosity level and a string
with the log message itself. See the separate [logging documentation](dbus-logging.md)
for details on this signal.

### `Properties`

| Name          | Type             | Read/Write | Description                                         |
|---------------|------------------|:----------:|-----------------------------------------------------|
| version       | string           | readonly   | Version of the currently running service            |

D-Bus destination: `net.openvpn.v3.configuration` \- Object path: `/net/openvpn/v3/configuration/${UNIQUE_ID}`
--------------------------------------------------------------------------------------------------------------

```
node /net/openvpn/v3/configuration/${UNIQUE_ID} {
  interface net.openvpn.v3.configuration {
    methods:
      Fetch(out s config);
      FetchJSON(out s config_json);
      SetOption(in  s option,
                in  s value);
      SetOverride(in  s name,
                  in  v value);
      UnsetOverride(in  s name);
      AccessGrant(in  u uid);
      AccessRevoke(in  u uid);
      Seal();
      Remove();
    signals:
    properties:
      readonly u owner;
      readonly au acl;
      readonly s name;
      readwrite b public_access;
      readonly b persistent;
      readonly t import_timestamp;
      readonly t last_used_timestamp;
      readwrite b locked_down;
      readonly a{sv} overrides;
      readonly b readonly;
      readonly b single_use;
      readwrite b transfer_owner_session;
      readonly u used_count;
      readwrite b dco;
      readonly b valid;
  };
};
```

### Method: `net.openvpn.v3.configuration.Fetch`

This method will return a string of the stored configuration profile
as it is stored. This should be contain the same information which was
imported.  It will not necessarily be an identical copy of what was
imported, as it has been processed during the import.

#### Arguments

| Direction | Name        | Type        | Description                                    |
|-----------|-------------|-------------|------------------------------------------------|
| Out       | config      | string      | The configuration file as a plain string blob. |


### Method: `net.openvpn.v3.configuration.FetchJSON`

This is a variant of Fetch, which returns the configuration profile
formatted as a JSON string blob. The intention of this is for user
front-ends to have a simple API to retrieve the complete configuration
profile in a format which can easily be parsed and presented in a user
interface.


#### Arguments

| Direction | Name        | Type        | Description                                             |
|-----------|-------------|-------------|---------------------------------------------------------|
| Out       | config      | string      | The configuration file as a JSON formatted string blob. |


### Method: `net.openvpn.v3.configuration.SetOption`

This method allows manipulation of a stored configuration. This is
targeted at user front-ends to be able to easily manipulate imported
configuration files.

** WARNING: ** This method is currently not implemented!

#### Arguments

| Direction | Name        | Type        | Description                                             |
|-----------|-------------|-------------|---------------------------------------------------------|
| In        | option      | string      | String containing the name of the option to be modified |
| In        | value       | string      | String containing the new value of the option           |


### Method: `net.openvpn.v3.configuration.AccessGrant`

By default, only the user ID (UID) who imported the configuration have
access to it.  This method used to grant other users access to the
configuration.

#### Arguments

| Direction | Name | Type         | Description                                         |
|-----------|------|--------------|-----------------------------------------------------|
| In        | uid  | unsigned int | The UID to the user account which is granted access |


### Method: `net.openvpn.v3.configuration.AccessRevoke`

This revokes access to a configuration object for a specific user.
Please note that the owner (the user which imported the configuration)
cannot have its access revoked.

#### Arguments

| Direction | Name | Type         | Description                                               |
|-----------|------|--------------|-----------------------------------------------------------|
| In        | uid  | unsigned int | The UID to the user account which gets the access revoked |


### Method: `net.openvpn.v3.configuration.Seal`

This method makes the configuration read-only. That means it can no
longer be manipulated, nor removed.

#### Arguments

(No arguments)

### Method: `net.openvpn.v3.configuration.Remove`

Removes a VPN profile from the configuration manager. If the
configuration is persistent, it will be removed from the disk as
well. This method takes no arguments and does not return anything on
success. If an error occurs, a D-Bus error is returned.

#### Arguments

(No arguments)

### `Properties`

| Name          | Type             | Read/Write | Description                                         |
|---------------|------------------|:----------:|-----------------------------------------------------|
| owner         | unsigned integer | Read-only  | The UID value of the user which did the import      |
| acl           | array(integer)   | Read-only  | An array of UID values granted access               |
| name          | string           | Read-only  | Contains the user friendly name of the configuration profile |
| public_access | boolean          | Read/Write | If set to true, access control is disabled. But only owner may change this property, modify the ACL or delete the configuration |
| persistent    | boolean          | Read-only  | If set to true, this configuration will be saved to disk by the configuration manager. The location of the file storage is managed by the configuration manager itself and the configuration manager will load persistent profiles each time it starts |
| import_timestamp | uint64        | Read-only  | Unix Epoch timestamp of the import time             |
| last_used_timestamp | uint64     | Read-only  | Unix Epoch timestamp of the last time it Fetch was called [1] |
| locked_down   | boolean          | Read/Write | If set to true, only the owner and openvpn user can retrieve the configuration file.  Other users granted access can only use this profile to start a new tunnel |
| overrides     | dictionary       | Read-only  | Contains all the override settings enabled.  This is stored as a key/value based dictionary, where value can be any arbitrary data type |
| readonly      | boolean          | Read-only  | If set to true, the configuration have been sealed and can no longer be modified |
| single_use    | boolean          | Read-only  | If set to true, this configuration profile will be automatically removed after the first `Fetch` call. This is intended to be used by command line clients providing a similar user experience as the OpenVPN 2.x versions provides. |
| transfer_owner_session | boolean | Read/Write | If set to true, another user granted access to this profile will transfer the VPN session ownership back to the profile owner at start up | 
| used_count    | unsigned integer | Read-only  | Number of times Fetch has been called [1]           |
| dco           | boolean          | Read/Write | If set to true, the VPN tunnel will make use of the kernel accellerated Data Channel Offload feature (requires kernel support) |
| valid         | boolean          | Read-only  | Contains an indication if the configuration profile is considered functional for a VPN session |

  [1] It will track/count ``Fetch`` usage only if the calling user is ``openvpn``
