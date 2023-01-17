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

> **_Important!_**
> 
> If using the `/var/log/halon/` folder as in the sample below and it does not exist, when you create it - give it the same permission as the `smtpd` process is using. Eg.
> ```
> mkdir /var/log/halon
> chown halon:staff /var/log/halon
> ```
> 

### smtpd.yaml

````
plugins:
  - id: logger
    config:
      logs:
        - id: mylog
          path: /var/log/halon/mylog.log
          fsync: false
```

### Log rotation

You can use this plugin with logrotate. A sample configuration could look like this.

```
/var/log/halon/test.log {
    su halon halon
    create 640 halon halon
    compress
    rotate 30
    daily
    missingok
    notifempty
    postrotate
        /opt/halon/bin/halonctl plugin command logger reopen:test
    endscript
}
```

## Exported functions

These functions needs to be [imported](https://docs.halon.io/hsl/structures.html#import) from the `extras://logger` module path.

### logger(id, data)

Log data to a log file based on its ID.

**Params**

- id `string` - The file ID
- data `string` - The data

**Returns**

Returns `true` if the data was logged successfully. On error `none` is returned.

**Example**

```
import { logger } from "extras://logger";
logger("mylog", "hello\n");
```

## Autoload

This plugin creates files needed and used by the `smtpd` process, hence this plugin does not autoload when running the `hsh` script interpreter. There are two issues that may occur

1) Bad file permission if logs are created by the user running `hsh` not the `smtpd` process
2) Log files may be corrupted if multiple processes are writing to the same log file simultaneously (`smtpd` and `hsh`). Do use this plugin in `hsh` if `smtpd` is running and vice versa.

To overcome the first issue, run `hsh` as `root` and use the `--privdrop` flag to become the same user as `smtpd` is using.

```
hsh --privdrop --plugin logger
````