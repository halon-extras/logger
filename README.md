# Logger plugin

This plugin allows you to define custom log files which may be used from HSL (```logger(id, data)```). The file may be reopened after a rotate using the halonctl command.

```
# halonctl plugin command logger reopen:mylog1
```

## Installation

Follow the [instructions](https://docs.halon.io/manual/comp_install.html#installation) in our manual to add our package repository and then run the below command.

### Ubuntu

```
apt-get install halon-extras-logger
```

### RHEL

```
yum install halon-extras-logger
```

## Configuration
For the configuration schema, see [logger.schema.json](logger.schema.json).

## Exported functions

### logger(id, data)

Log data to a log file based on its ID.

**Params**

- id `string` - The file ID
- data `string` - The data

**Returns**

Returns `true` if the data was logged successfully. On error `none` is returned.
