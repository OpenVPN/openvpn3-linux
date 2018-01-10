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
    signals:
      Log(u group,
          u level,
          s message);
    properties:
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


### Signal: `net.openvpn.v3.configuration.Log`

Whenever the configuration manager want to log something, it issues a
Log signal which carries a log group, log verbosity level and a string
with the log message itself. See the separate [logging documentation](dbus-logging.md)
for details on this signal.


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
      AccessGrant(in  u uid);
      AccessRevoke(in  u uid);
      Seal();
      Remove();
    signals:
    properties:
      readonly u owner = 2001;
      readonly au acl = [];
      readonly s name;
      readonly b valid;
      readonly b readonly;
      readonly b single_use;
      readonly b persistent;
      readwrite b public_access = false;
      readwrite s alias;
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
| readonly      | boolean          | Read-only  | If set to true, the configuration have been sealed and can no longer be modified |
| single_use    | boolean          | Read-only  | If set to true, this configuration profile will be automatically removed after the first `Fetch` call. This is intended to be used by command line clients providing a similar user experience as the OpenVPN 2.x versions provides. |
| peristent     | boolean          | Read-only  | If set to true, this configuration will be saved to disk by the configuration manager. The location of the file storage is managed by the configuration manager itself and the configuration manager will load persistent profiles each time it starts |
| public_access | boolean          | Read/Write | If set to true, access control is disabled. But only owner may change this property, modify the ACL or delete the configuration |
| alias         | string           | Read/Write | This can be used to have a more user friendly reference to a VPN profile than the D-Bus object path. This is primarily intended for command line interfaces where this alias name can be used instead of the full unique D-Bus object path to this VPN profile |
