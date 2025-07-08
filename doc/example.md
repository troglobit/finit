Configuration Example
=====================

This example `/etc/finit.conf` can also be split up in multiple `.conf`
files in `/etc/finit.d`.  Available, but not yet enabled, services can
be placed in `/etc/finit.d/available` and enabled by an operator using
the [initctl](initctl.md) tool.

See the [contrib/][contrib] directory on GitHub for examples, or take a
peek at systems using Finit, like [Infix OS][infix] and [myLinux][].

> [!TIP]
> As of Finit v4.4, `.conf` lines can be broken up using the standard UNIX
> continuation character (`\`), trailing comments are also supported.  The
> latter means you must escape any hashes used in directives and descriptions
> (`\#`).  For more on this and examples, see the [finit.conf(5)][] manual or
> the [Configuration](config/index.md) section.

```ApacheConf
# Fallback if /etc/hostname is missing
host default

# Runlevel to start after bootstrap, 'S', default: 2
#runlevel 2

# Support for setting global environment variables, using foo=bar syntax
# be careful though with variables like PATH, SHELL, LOGNAME, etc.
#PATH=/usr/bin:/bin:/usr/sbin:/sbin

# Max file size for each log file: 100 kiB, rotate max 4 copies:
# log => log.1 => log.2.gz => log.3.gz => log.4.gz
log size=100k count=4

# Services to be monitored and respawned as needed
service [S12345] env:-/etc/conf.d/watchdog watchdog $WATCHDOG_OPTS $WATCHDOG_DEV -- System watchdog daemon
service [S12345] env:-/etc/conf.d/syslog syslogd -n $SYSLOGD_OPTS          -- System log daemon
service [S12345] <pid/syslogd> env:-/etc/conf.d/klogd klogd -n $KLOGD_OPTS -- Kernel log daemon
service   [2345] env:-/etc/conf.d/lldpd lldpd -d $LLDPD_OPTS               -- LLDP daemon (IEEE 802.1ab)

# The BusyBox ntpd does not use syslog when running in the foreground
# So we use this trick to redirect stdout/stderr to a log file.  The
# log file is rotated with the above settings.  The condition declares
# a dependency on a system default route (gateway) to be set.  A single
# <!> at the beginning means ntpd does not respect SIGHUP for restart.
service [2345] log:/var/log/ntpd.log <!net/route/default> ntpd -n -l -I eth0 -- NTP daemon

# For multiple instances of the same service, add :ID somewhere between
# the service/run/task keyword and the command.
service :80   [2345] merecat -n -p 80   /var/www -- Web server
service :8080 [2345] merecat -n -p 8080 /var/www -- Old web server

# Alternative method instead of below runparts, can also use /etc/rc.local
#sysv [S] /etc/init.d/keyboard-setup       -- Setting up preliminary keymap
#sysv [S] /etc/init.d/acpid                -- Starting ACPI Daemon
#task [S] /etc/init.d/kbd                  -- Preparing console

# Hidden from boot progress, using empty `--` description
#sysv [S] /etc/init.d/keyboard-setup       --
#sysv [S] /etc/init.d/acpid                --
#task [S] /etc/init.d/kbd                  --

# Run start scripts from this directory
# runparts /etc/start.d

# Virtual consoles run BusyBox getty, keep kernel default speed
tty [12345] /sbin/getty -L 0 /dev/tty1  linux nowait noclear
tty [2345]  /sbin/getty -L 0 /dev/tty2  linux nowait noclear
tty [2345]  /sbin/getty -L 0 /dev/tty3  linux nowait noclear

# Use built-in getty for serial port and USB serial
#tty [12345] /dev/ttyAMA0 noclear nowait
#tty [12345] /dev/ttyUSB0 noclear

# Just give me a shell, I need to debug this embedded system!
#tty [12345] console noclear nologin
```

The `service` stanza, as well as `task`, `run` and others are described in
full in the [Services Syntax](config/services.md) section.

Here's a quick overview of some of the most common components needed to start
a UNIX daemon:

```
service [LVLS] <COND> log env:[-]/etc/default/daemon daemon ARGS -- Example daemon
^       ^      ^      ^   ^                          ^      ^       ^
|       |      |      |   |                          |      |        `---------- Optional description
|       |      |      |   |                          |       `------------------ Daemon arguments
|       |      |      |   |                           `------------------------- Path to daemon
|       |      |      |    `---------------------------------------------------- Optional env. file
|       |      |       `-------------------------------------------------------- Redirect output to log
|       |       `--------------------------------------------------------------- Optional conditions
|        `---------------------------------------------------------------------- Optional Runlevels
 `------------------------------------------------------------------------------ Supervised program (daemon)
```

Some components are optional: runlevel(s), condition(s) and description,
making it easy to create simple start scripts and still possible for more
advanced uses as well:

    service /usr/sbin/sshd -D

Dependencies are handled using [conditions](conditions.md).  One of
the most common conditions is to wait for basic networking to become
available:

    service <net/route/default> nginx -- High performance HTTP server

Here is another example where we instruct Finit to not start BusyBox
`ntpd` until `syslogd` has started properly.  Finit waits for `syslogd`
to create its PID file, by default `/var/run/syslogd.pid`.

    service [2345] log <!pid/syslogd> ntpd -n -N -p pool.ntp.org
    service [S12345] syslogd -n -- Syslog daemon

Notice the `log` keyword, BusyBox `ntpd` uses `stderr` for logging when
run in the foreground.  With `log` Finit redirects `stdout` + `stderr`
to the system log daemon using the command line `logger(1)` tool.

A service, or task, can have multiple dependencies listed.  Here we wait
for *both* `syslogd` to have started and basic networking to be up:

    service [2345] log <pid/syslogd,net/route/default> ntpd -n -N -p pool.ntp.org

If either condition fails, e.g. loss of networking, `ntpd` is stopped
and as soon as it comes back up again `ntpd` is restarted automatically.

> [!NOTE]
> Make sure daemons do *not* fork and detach themselves from the controlling
> TTY, usually an `-n` or `-f` flag, or `-D` as in the case of OpenSSH above.
> If it detaches itself, Finit cannot monitor it and will instead try to
> restart it.

[finit.conf(5)]: https://man.troglobit.com/man5/finit.conf.5.html
[infix]:         https://kernelkit.github.io
[myLinux]:       https://github.com/troglobit/myLinux/
[contrib]:       https://github.com/troglobit/finit/tree/master/contrib
