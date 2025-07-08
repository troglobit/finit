General Logging
===============

**Syntax:** `log size:200k count:5`

Log rotation for run/task/services using the `log` sub-option with
redirection to a log file.  Global setting, applies to all services.

The size can be given as bytes, without a specifier, or in `k`, `M`,
or `G`, e.g. `size:10M`, or `size:3G`.  A value of `size:0` disables
log rotation.  The default is `200k`.

The count value is recommended to be between 1-5, with a default 5.
Setting count to 0 means the logfile will be truncated when the MAX
size limit is reached.

Redirecting Output
------------------

The `run`, `task`, and `service` stanzas also allow the keyword `log` to
redirect `stderr` and `stdout` of the application to a file or syslog
using the native `logit` tool.  This is useful for programs that do not
support syslog on their own, which is sometimes the case when running
in the foreground.

The full syntax is:

    log:/path/to/file
    log:prio:facility.level,tag:ident
    log:console
    log:null
    log

Default `prio` is `daemon.info` and default `tag` is the basename of the
service or run/task command.

Log rotation is controlled using the global `log` setting.

**Example:**

    service log:prio:user.warn,tag:ntpd /sbin/ntpd pool.ntp.org -- NTP daemon
