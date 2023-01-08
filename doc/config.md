Finit Configuration
===================

* [Introduction](#introduction)
  * [Environment Variables](#environment-variables)
* [Service Environment](#service-environment)
* [Service Wrapper Scripts](#service-wrapper-scripts)
* [Cgroups](#cgroups)
* [Configuration File Syntax](#configuration-file-syntax)
  * [Hostname](#hostname)
  * [Kernel Modules](#kernel-modules)
  * [Networking](#networking)
  * [Alternate rcS.d/](#alternate-rcs-d-)
  * [Resource Limits](#resource-limits)
  * [Runlevels](#runlevels)
  * [One-shot Commands (sequence)](#one-shot-commands--sequence-)
  * [One-shot Commands (parallel)](#one-shot-commands--parallel-)
  * [SysV Init Scripts](#sysv-init-scripts)
  * [Services](#services)
  * [Run-parts Scripts](#run-parts-scripts)
  * [Including Finit Configs](#including-finit-configs)
  * [General Logging](#general-logging)
  * [TTYs and Consoles](#ttys-and-consoles)
  * [Non-privileged Services](#non-privileged-services)
  * [Redirecting Output](#redirecting-output)
* [Rescue Mode](#rescue-mode)
* [Limitations](#limitations)
  * [/etc/finit.conf](#etcfinitconf)
  * [/etc/finit.d](#etcfinitd)
* [Watchdog](#watchdog)
* [keventd](#keventd)


Introduction
------------

Finit can be configured using only the original `/etc/finit.conf` file
or in combination with `/etc/finit.d/*.conf`.  Useful for package-based
Linux distributions -- each package can provide its own "script" file.

- `/etc/finit.conf`: main configuration file
- `/etc/finit.d/*.conf`: snippets, usually one service per file

> **Note:** the above .conf file name and rcS.d directory name are the
> defaults.  They can be changed at compile-time with two `configure`
> options: `--with-config=FOO` and `--with-rcsd=BAR`.
>
> They can also be overridden from the command line `finit.config=BAZ`
> and the top-level configuration directive `rcsd /path/to/finit.d`.

Not all configuration directives are available in `/etc/finit.d/*.conf`
and some directives are only available at [bootstrap][], runlevel `S`,
see the section [Limitations](#limitations) below for details.

To add a new service, drop a `.conf` file in `/etc/finit.d` and run
`initctl reload`.  (It is also possible to `SIGHUP` to PID 1, or call
`finit q`, but that has been deprecated with the `initctl` tool).  Finit
monitors all known active `.conf` files, so if you want to force a
restart of any service you can touch its corresponding `.conf` file in
`/etc/finit.d` and call `initctl reload`.  Finit handle any and all
conditions and dependencies between services automatically.

It is also possible to drop `.conf` files in `/etc/finit.d/available/`
and use `initctl enable` to enable a service `.conf` file.  This may be
useful in particular to Linux distributions that may want to install all
files for a package and let the user decide when to enable a service.

On `initctl reload` the following is checked for all services:

- If a service's `.conf` file has been removed, or its conditions are no
  longer satisfied, the service is stopped.
- If the file is modified, or a service it depends on has been reloaded,
  the service is reloaded (stopped and started).
- If a new service is added it is automatically started — respecting
  runlevels and return values from any callbacks.

For more info on the different states of a service, see the separate
document [Finit Services](service.md).

> When running <kbd>make install</kbd> no default `/etc/finit.conf` is
> installed since system requirements differ too much.  There are some
> examples in the `contrib/` directory, which can be used as a base.


### Environment Variables

In Finit v4.3 support for setting environment variables in `finit.conf`,
and any `*.conf`, was added.  It is worth noting that these are global
and *shared with all* services -- the only way to have service-local
environment is detailed in the next section.

The syntax for global environment variables is straight forward:

    foo=bar
    baz="qux"

On reload of .conf files, all tracked environment variables are cleared
so if `foo=bar` is removed from `finit.conf`, or any `finit.d/*.conf`
file, it will no longer be used by Finit or any new (!) started
run/tasks or services.  The environment of already started processes can
not be changed.

The only variables reset to sane defaults on .conf reload are:

    PATH=_PATH_STDPATH
    SHELL=_PATH_BSHELL
    LOGNAME=root
    USER=root

It is entirely possible to override these as well from the .conf files,
but be careful.  Changing SHELL changes the behavior of `system()` and a
lot of other commands as well.


Service Environment
-------------------

Finit supports sourcing environment variables from `/etc/default/*`, or
similar `--with-sysconfig=DIR`.  This is a common pattern from SysV init
scripts, where the start-stop script is a generic script for the given
service, `foo`, and the options for the service are sourced from the
file `/etc/default/foo`.  Like this:

* `/etc/default/foo`:

        FOO_OPTIONS=--extra-arg="bar" -s -x

* `/etc/finit.conf`:

        service [2345] env:-/etc/default/foo foo -n $FOO_OPTIONS -- Example foo daemon

Here the service `foo` is started with `-n`, to make sure it runs in the
foreground, and the with the options found in the environment file.  With
the `ps` command we can see that the process is started with:

    foo -n --extra-arg=bar -s -x

> **Note:** the leading `-` determines if Finit should treat a missing
> environment file as blocking the start of the service or not.  When
> `-` is used, a missing environment file does *not* block the start.


Service Synchronization
-----------------------

Finit was created for fast booting systems.  Faster than a regular SysV
Init based system at the time.  Early on the need for a guaranteed start
order of services (daemons) arose.  I.e., service `A` must be guaranteed
to have started (and be ready!) before `B`.  The model that was chosen
to determine this was very simple: PID files.

Early on in UNIX daemons were controlled with basic IPC like signals,
and the way for a user to know that a daemon was ready to respond to
signals (minimally having set up its signal handler), was to tell the
user;

> "Hey, you can send signals to me using the PID in this file:
> `/var/run/daemon.pid`".

Since most systems run fairly unchanged after boot, Finit could rely on
the PID file for `A` being created before launching `B`.  This method
has worked well for a long time, and for systems based on Open Source it
was easy to either add PID file support to a daemon without support for
it, or fix ordering issues (PID file created before signal handler is
set up) in existing daemons.

However, with the advent of other Init systems (Finit is rather old),
most notably systemd and s6, other methods for signaling "readiness"
arrived and daemons were adapted to these new schemes to a larger
extent.

As of Finit v4.4 partial support for systemd and s6 is available.  The
native PID file mode of operation is still default, the other's can be
defined per service, like this:

    service notify:systemd foo
    service notify:s6      bar

To synchronize two services the following condition can be used:

    service notify:systemd    A
    service <service/A/ready> B

For details on the syntax and options, see below.

> **Note:** on `initctl reload` conditions are normally set in "flux",
> while figuring out which to stop, start or restart.  Services that
> need to be restarted have their `ready` condition removed prior to
> Finit sending them SIGHUP (if they support that), or stop-starting
> them.  A daemon is expected to reassert its readiness, e.g. systemd
> style daemons to write `READY=1\n`.
>
> However, the s6 notify mode does not support this because in s6 you
> are expected to close your notify descriptor after having written
> `\n`.  This means s6 style daemons currently must be stop-started.
> (Declare the service with `<!>` in its condition statement.)
>
> For native PID file style readiness notification a daemon is expected
> to either create its PID file, if it does not exist yet, or touch it
> using `utimensat()` or similar.  This triggers both the `<pid/>` and
> `<.../ready>` conditions.


Service Wrapper Scripts
-----------------------

If your service requires to run additional commands, executed before the
service is actually started, like the systemd `ExecStartPre`, you can
use a wrapper shell script to start your service.

The Finit service `.conf` file can be put into `/etc/finit.d/available`,
so you can control the service using `initctl`.  Then use the path to
the wrapper script in the Finit `.conf` service stanza.  The following
example employs a wrapper script in `/etc/start.d`.

**Example:**

* `/etc/finit.d/available/program.conf`:

        service [235] <!> /etc/start.d/program -- Example Program

* `/etc/start.d/program:`

        #!/bin/sh
        # Prepare the command line options
        OPTIONS="-u $(cat /etc/username)"

        # Execute the program
        exec /usr/bin/program $OPTIONS

> **Note:** the example sets `<!>` to denote that it doesn't support
>           `SIGHUP`.  That way Finit will stop/start the service
>           instead of sending SIGHUP at restart/reload events.


Cgroups
-------

There are three major cgroup configuration directives:

 1. Global top-level group: init, system, user, or a custom group
 2. Selecting a top-level group for a set of run/task/services
 3. Per run/task/service limits

Top-level group configuration.

    # Top-level cgroups and their default settings.  All groups mandatory
    # but more can be added, max 8 groups in total currently.  The cgroup
    # 'root' is also available, reserved for RT processes.  Settings are
    # as-is, only one shorthand 'mem.' exists, other than that it's the
    # cgroup v2 controller default names.
    cgroup init   cpu.weight:100
    cgroup user   cpu.weight:100
    cgroup system cpu.weight:9800

Adding an extra cgroup `maint/` will require you to adjust the weight of
the above three.  We leave `init/` and `user/` as-is reducing weight of
`system/` to 9700.

    cgroup system cpu.weight:9700

    # Example extra cgroup 'maint'
    cgroup maint  cpu.weight:100

By default, the `system/` cgroup is selected for almost everything.  The
`init/` cgroup is reserved for PID 1 itself and its closest relatives.
The `user/` cgroup is for local TTY logins spawned by getty.

To select a different top-level cgroup, e.g. `maint/`, one can either
define it for a group of run/task/service directives in a `.conf` or per
each stanza:

    cgroup.maint
    service [...] <...> /path/to/foo args -- description
    service [...] <...> /path/to/bar args -- description

or

    service [...] <...> cgroup.maint /path/to/foo args -- description

The latter form also allows per-stanza limits on the form:

    service [...] <...> cgroup.maint:cpu.max:10000,mem.max:655360 /path/to/foo args -- description

Notice the comma separation and the `mem.` exception to the rule: every
cgroup setting maps directly to cgroup v2 syntax.  I.e., `cpu.max` maps
to the file `/sys/fs/cgroup/maint/foo/cpu.max`.  There is no filtering,
except for expanding the shorthand `mem.` to `memory.`, if the file is
not available, either the cgroup controller is not available in your
Linux kernel, or the name is misspelled.

> Linux cgroups and details surrounding values are not explained in the
> Finit documentation.  The Linux admin-guide cover this well:
> <https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html>


Configuration File Syntax
-------------------------

The file format is line based, empty lines and comments, lines starting
with `#`, are ignored.  A configuration directive starts with a keyword
followed by a space and the rest of the line is treated as the value.

As of Finit v4.4, configuration directives can be broken up in multiple
lines using the continuation character `\`, and trailing comments are
also allowed.  Example:

```aconf
# Escape \# chars if you want them literal in, e.g., descriptions
service name:sysklogd [S123456789]   \
    env:-/etc/default/sysklogd       \
    syslogd -F $SYSLOGD_ARGS         \
    -- System log daemon \# 1   # Comments allowed now
```

The .conf files `/etc/finit.conf` and `/etc/finit.d/*` support the
following directives:

### Hostname

**Syntax:** `host <NAME>`, or `hostname <NAME>`

Set system hostname to NAME, unless `/etc/hostname` exists in which case
the contents of that file is used.

Deprecated.  We recommend using `/etc/hostname` instead.

> **Note:** only read and executed in runlevel S ([bootstrap][]).


### Kernel Modules

**Syntax:** `module <MODULE> [ARGS]`

Load a kernel module, with optional arguments.  Similar to `insmod`
command line tool.

Deprecated, there is both a `modules-load.so` and a `modprobe.so` plugin
that can handle module loading better.  The former supports loading from
`/etc/modules-load.d/`, the latter uses kernel modinfo to automatically
load (or coldplug) every required module.  For hotplug we recommend the
BusyBox mdev tool, add to `/etc/mdev.conf`:

    $MODALIAS=.*  root:root       0660    @modprobe -b "$MODALIAS"

> **Note:** only read and executed in runlevel S ([bootstrap][]).


### Networking

**Syntax:** `network <PATH>`

Script or program to bring up networking, with optional arguments.

Deprecated.  We recommend using dedicated task/run stanzas per runlevel,
or `/etc/network/interfaces` if you have a system with `ifupdown`, like
Debian, Ubuntu, Linux Mint, or an embedded BusyBox system.

> **Note:** only read and executed in runlevel S ([bootstrap][]).


### Alternate finit.d/

**Syntax:** `rcsd /path/to/finit.d`

The Finit rcS.d directory is set at compile time with:

    ./configure --with-rcsd=/etc/finit.d

A system with multiple use-cases may be bootstrapped with different
configurations, starting with the kernel command line option:

    -- finit.config=/etc/factory.conf

This file in turn can use the `rcsd` directive to tell Finit to use
another set of .conf files, e.g.:

    rcsd /etc/factory.d

> **Note:** this directive is only available from the top-level
> bootstrap .conf file.

### Resource Limits

**Syntax:** `rlimit [hard|soft] RESOURCE <LIMIT|unlimited>`

Set the hard or soft limit for a resource, or both if that argument is
omitted.  `RESOURCE` is the lower-case `RLIMIT_` string constants from
`setrlimit(2)`, without prefix.  E.g. to set `RLIMIT_CPU`, use `cpu`.
  
LIMIT is an integer that depends on the resource being modified, see
the man page, or the kernel `/proc/PID/limits` file, for details.
Finit versions before v3.1 used `infinity` for `unlimited`, which is
still supported, albeit deprecated.

    # No process is allowed more than 8MB of address space
    rlimit hard as 8388608

    # Core dumps may be arbitrarily large
    rlimit soft core infinity

    # CPU limit for all services, soft & hard = 10 sec
    rlimit cpu 10

`rlimit` can be set globally, in `/etc/finit.conf`, or locally per
each `/etc/finit.d/*.conf` read.  I.e., a set of task/run/service
stanzas can share the same rlimits if they are in the same .conf.


### Runlevels

**Syntax:** `runlevel <N>`

The system runlevel to go to after [bootstrap][] (S) has completed.  `N`
is the runlevel number 0-9, where 6 is reserved for reboot and 0 for
halt.

*Default:* 2

> **Note:** only read and executed in runlevel S ([bootstrap][]).


### One-shot Commands (sequence)

**Syntax:** `run [LVLS] <COND> /path/to/cmd ARGS -- Optional description`

One-shot command to run in sequence when entering a runlevel, with
optional arguments and description.
  
`run` commands are guaranteed to be completed before running the next
command.  Highly useful if true serialization is needed.

> `<COND>` is described in the [Services](#services) section.


### One-shot Commands (parallel)

**Syntax:** `task [LVLS] <COND> /path/to/cmd ARGS -- Optional description`

One-shot like 'run', but starts in parallel with the next command.
  
Both `run` and `task` commands are run in a shell, so pipes and
redirects can be freely used:

    task [s] echo "foo" | cat >/tmp/bar

> `<COND>` is described in the [Services](#services) section.


### SysV Init Scripts

**Syntax:** `sysv [LVLS] <COND> /path/to/init-script -- Optional description`

Similar to `task` is the `sysv` stanza, which can be used to call SysV
style scripts.  The primary intention for this command is to be able to
re-use much of existing setup and init scripts in Linux distributions.
  
When entering an allowed runlevel, Finit calls `init-script start`, when
entering a disallowed runlevel, Finit calls `init-script stop`, and if
the Finit .conf, where `sysv` stanza is declared, is modified, Finit
calls `init-script restart` on `initctl reload`.  Similar to how
`service` stanzas work.

Forking services started with `sysv` scripts can be monitored by Finit
by declaring the PID file to look for: `pid:!/path/to/pidfile.pid`.
Notice the leading `!`, it signifies Finit should not try to create the
file, but rather watch that file for the resulting forked-off PID.  This
syntax also works for forking daemons that do not have a command line
option to run it in the foreground, more on this below in `service`.

> `<COND>` is described in the [Services](#services) section.


### Services

**Syntax:** `service [LVLS] <COND> /path/to/daemon ARGS -- Optional description`

Service, or daemon, to be monitored and automatically restarted if it
exits prematurely.  Finit tries to restart services that die, by
default 10 times before giving up and marking them as *crashed*.
After which they have to be manually restarted with `initctl restart
NAME`.  The limits controling this are configurable, see the options
below.

> **Tip:** to allow endless restarts, see below option `respawn`
  
For daemons that support it, we recommend appending `--foreground`,
`--no-background`, `-n`, `-F`, or similar command line argument to
prevent them from forking off a sub-process in the background.  This is
the most reliable way to monitor a service.

However, not all daemons support running in the foreground, or they may
start logging to the foreground as well, these are forking daemons and
are supported using the same syntax as forking `sysv` services, using
the `pid:!/path/to/pidfile.pid` syntax.  There is an alternative syntax
that may be more intuitive, where Finit can also guess the PID file
based on the daemon's command name:

    service type:forking ntpd -- NTP daemon

This example lets BusyBox `ntpd` daemonize itself.  Finit uses the
basename of the binary to guess the PID file to watch for the PID:
`/var/run/ntpd.pid`.  If Finit guesses wrong, you have to submit the
full `pid:!/path/to/file.pid`.

  
**Example:**

In the case of `ospfd` (below), we omit the `-d` flag (daemonize) to
prevent it from forking to the background:

    service [2345] <pid/zebra> /sbin/ospfd -- OSPF daemon

`[2345]` denote the runlevels `ospfd` is allowed to run in, they are
optional and default to level 2-5 if omitted.
  
`<...>` is the condition for starting `ospfd`.  In this example Finit
waits for another service, `zebra`, to have created its PID file in
`/var/run/quagga/zebra.pid` before starting `ospfd`.  Finit watches
*all* files in `/var/run`, for each file named `*.pid`, or `*/pid`,
Finit opens it and find the matching `NAME:ID` using the PID.

Some services do not maintain a PID file and rather than patching each
application Finit provides a workaround.  A `pid` keyword can be set
to have Finit automatically create (when starting) and later remove
(when stopping) the PID file.  The file is created in the `/var/run`
directory using the `basename(1)` of the service.  The default can be
modified with an optional `pid:`-argument:

    pid[:[/path/to/]filename[.pid]]

For example, by adding `pid:/run/bar.pid` to the service `/sbin/bar`,
that PID file will, not only be created and removed automatically, but
also be used by the Finit condition subsystem.  So a service/run/task
can depend on `<pid/bar>`, like this foo will not be started until bar
has started:

    service pid:/run/bar.pid bar -- Bar Service
    service <pid/bar> foo -- Foo Service

Needless to say, it is better if `bar` creates its own PID file when it
has completed starting up and is ready for service.

As an alternative "readiness" notification, Finit supports both systemd
and s6 style notification.  This can be enabled by using the `notify`
option:

  * `notify:systemd` -- tells Finit the service uses the `sd_notify()`
    API to signal PID 1 when it has completed its startup and is ready
    to service events.  The [sd_notify()][] API expects `NOTIFY_SOCKET`
    to be set to the socket where the application can send `"READY=1\n"`
    when it is starting up or has processed a `SIGHUP`.
  * `notify:s6` -- puts Finit in s6 compatibility mode.  Compared to the
    systemd notification, [s6 expect][] compliant daemons to send `"\n"`
    and then close their socket.  Finit takes care of "hard-wiring" the
    READY state as long as the application is running, events across any
    `SIGHUP`.  Since s6 can give its applications the descriptor number
    (must be >3) on then command line, Finit provides the following
    syntax (`%n` is replaced by Finit with then descriptor number):

        service [S12345789] notify:s6 mdevd -O 4 -D %n

[sd_notify()]: https://www.freedesktop.org/software/systemd/man/sd_notify.html
[s6 expect]:   https://skarnet.org/software/s6/notifywhenup.html

When a service is ready, either by Finit detecting its PID file, or
their respective readiness mechanism has been triggered, Finit creates
then service's ready condition which other services can depend on:

    $ initctl -v cond get service/mdevd/ready
    on

This can be used to synchronize the start of another run/task/service:

    task [S] <service/mdevd/ready> @root:root mdevd-coldplug

Finit waits for `mdevd` to notify it, before starting `mdevd-coldplug`.
Notice how both start in runlevel S, and the coldplug task only runs in
S.  When the system moves to runlevel 2 (the default), coldplug is no
longer part of the running configuration (`initctl show`), this is to
ensure that coldplug is not called more than once.

>  For a detailed description of conditions, and how to debug them,
>  see the [Finit Conditions](conditions.md) document.

If a service should not be automatically started, it can be configured
as manual with the optional `manual` argument. The service can then be
started at any time by running `initctl start <service>`.

    manual:yes

The name of a service, shown by the `initctl` tool, defaults to the
basename of the service executable. It can be changed with the
optional `name` argument:

    name:<service-name>

As mentioned previously, services are automatically restarted, this is
configurable with the following options:

  * `restart:NUM` -- number of times Finit tries to restart a crashing
    service, default: 10, max: 255.  When this limit is reached the
    service is marked *crashed* and must be restarted manually with
    `initctl restart NAME`
  * `restart_sec:SEC` -- number of seconds before Finit tries to restart
    a crashing service, default: 2 seconds for the first five retries,
	then back-off to 5 seconds.  The maximum of this configured value
	and the above (2 and 5) will be used
  * `restart:always` -- no upper limit on the number of times Finit
    tries to restart a crashing service.  Same as `restart:-1`
  * `norestart` -- dont restart on failures, same as `restart:0`
  * `respawn` -- bypasses the `restart` mechanism completely, allows
    endless restarts.  Useful in many use-cases, but not what `service`
    was originally designed for so not the default behavior
  * `oncrash:reboot` -- when all retries have failed, and the service
    has *crashed*, if this option is set the system is rebooted.
  * `oncrash:script` -- similarly, but instead of rebooting, call the
    `post:script` action if set, see below.

When stopping a service (run/task/sysv/service), either manually or
when moving to another runlevel, Finit starts by sending `SIGTERM`, to
allow the process to shut down gracefully.  If the process has not
been collected within 3 seconds, Finit sends `SIGKILL`.  To halt the
process using a different signal, use the option `halt:SIGNAL`, e.g.,
`halt:SIGPWR`.  To change the delay between your halt signal and KILL,
use the option `kill:SEC`, e.g., `kill:10` to wait 10 seconds before
sending `SIGKILL`.

Services support `pre:script` and `post:script` actions as well.  These
run as the same `@USER:GROUP` as the service itself, with any `env:file`
sourced.  The scripts must use an absolute path, but are executed from
the `$HOME` of the given user.  The scripts are not called with any
argument (currently), but both get the `SERVICE_IDENT=foo` environment
variable set.  Here `foo` denotes the identity of the service, which if
there are multiple services named `foo`, may be `foo:1`, or any unique
identifier specified in the .conf file.  The `post:script` is called
with an additional set of environment variables:

 - `EXIT_CODE=[exited,signal]`: set to one of `exited` or `signal`
 - `EXIT_STATUS=[num,SIGNAME]`: set to one of exit status code from
   the program, if it exited normally, or the signal name (`HUP`,
   `TERM`, etc.) if it exited due to signal

These script actions *must terminate*, so they have a default execution
time of 3 seconds before they are SIGKILLed, this can be adjusted using
the above `kill:SEC` syntax.


### Run-parts Scripts

**Syntax:** `runparts <DIR>`

Call [run-parts(8)][] on `DIR` to run start scripts.  All executable
files, or scripts, in the directory are called, in alphabetic order.
The scripts in this directory are executed at the very end of runlevel
`S`, [bootstrap][].

**Limitations:**

Scripts called from `runparts`, or hook scripts (see below), are limited
in what they can do and expect from Finit:

  1. Finit is single threaded and all scripts executed from these two
     mechanisms are limited to file system operations (early or late
	 hooks may not have write access!) and standard UNIX programs.
  2. Thus, it is *not possible* to call `initctl` to query status of
     services since that calls Finit over the `/run/finit/socket`.
  3. Confusingly enough, there are some commands to `initctl` that are
     available; `ls`, `enable`, `disable`, `touch`, `create`, `delete`,
     and all the `cond` commands.  Because all these are basically just
     wrappers for manipulating files (again, write access for early/late
     hooks may not be available.)

Also, similar to the `/etc/rc.local` shell script, make sure that all
your services and programs either terminate or start in the background
or you will block Finit.

> **Note:** `runparts` scrips are only read and executed in runlevel S
> ([bootstrap][]).  See [hook scripts](plugins.md#hooks) for other ways
> to run scripts at certain points during the complete lifetime of the
> system.

**Recommendations:**

It can be beneficial to use `01-name`, `02-othername`, etc., to ensure
the scripts are started in that order, e.g., if there is a dependency
order between scripts.  Symlinks to existing daemons can talso be used,
but make sure they daemonize (background) themselves properly, otherwise
Finit will lock up.

If `S[0-9]foo` and `K[0-9]bar` style naming is used, the executable will
be called with an extra argument, `start` and `stop`, respectively.
E.g., `S01foo` will be called as `S01foo start`.  Of course, `S01foo`
and `K01foo` may be a symlink to to `another/directory/foo`.

[run-parts(8)]: http://manpages.debian.org/cgi-bin/man.cgi?query=run-parts


### Including Finit Configs

**Syntax:** `include <CONF>`

Include another configuration file.  Absolute path required.


### General Logging

**Syntax:** `log size:200k count:5`

Log rotation for run/task/services using the `log` sub-option with
redirection to a log file.  Global setting, applies to all services.

The size can be given as bytes, without a specifier, or in `k`, `M`,
or `G`, e.g. `size:10M`, or `size:3G`.  A value of `size:0` disables
log rotation.  The default is `200k`.

The count value is recommended to be between 1-5, with a default 5.
Setting count to 0 means the logfile will be truncated when the MAX
size limit is reached.

### TTYs and Consoles

**Syntax:** `tty [LVLS] <COND> DEV [BAUD] [noclear] [nowait] [nologin] [TERM]`  
  `tty [LVLS] <COND> CMD <ARGS> [noclear] [nowait]`  
  `tty [LVLS] <COND> [notty] [rescue]`

The first variant of this option uses the built-in getty on the given
TTY device DEV, in the given runlevels.  DEV may be the special keyword
`@console`, which is expanded from `/sys/class/tty/console/active`,
useful on embedded systems.

The default baud rate is 0, i.e., keep kernel default.

> The `tty` stanza inherits runlevel, condition (and other feature)
> parsing from the `service` stanza.  So TTYs can run in one or many
> runlevels and depend on any condition supported by Finit.  This is
> useful e.g. to depend on `<pid/elogind>` before starting a TTY.

**Example:**

    tty [12345] /dev/ttyAMA0 115200 noclear vt220

The second `tty` syntax variant is for using an external getty, like
agetty or the BusyBox getty.

The third variant is for board bringup and the `rescue` boot mode.  No
device node is required in this variant, the same output that the kernel
uses is reused for stdio.  If the `rescue` option is omitted, a shell is
started (`nologin`, `noclear`, and `nowait` are implied), if the rescue
option is set the bundled `/libexec/finit/sulogin` is started to present
a bare-bones root login prompt.  If the root (uid:0, gid:0) user does
not have a password set, no rescue is possible.  For more information,
see the [Rescue Mode](#rescue-mode) section.

By default, the first two syntax variants *clear* the TTY and *wait* for
the user to press enter before starting getty.

**Example:**

    tty [12345] /sbin/getty  -L 115200 /dev/ttyAMA0 vt100
    tty [12345] /sbin/agetty -L ttyAMA0 115200 vt100 nowait

The `noclear` option disables clearing the TTY after each session.
Clearing the TTY when a user logs out is usually preferable.
  
The `nowait` option disables the `press Enter to activate console`
message before actually starting the getty program.  On small and
embedded systems running multiple unused getty wastes both memory
and CPU cycles, so `wait` is the preferred default.

The `nologin` option disables getty and `/bin/login`, and gives the
user a root (login) shell on the given TTY `<DEV>` immediately.
Needless to say, this is a rather insecure option, but can be very
useful for developer builds, during board bringup, or similar.

Notice the ordering, the `TERM` option to the built-in getty must be
the last argument.

Embedded systems may want to enable automatic `DEV` by supplying the
special `@console` device.  This works regardless weather the system
uses `ttyS0`, `ttyAMA0`, `ttyMXC0`, or anything else.  Finit figures
it out by querying sysfs: `/sys/class/tty/console/active`.  The speed
can be omitted to keep the kernel default.

> Most systems get by fine by just using `console`, which will evaluate
> to `/dev/console`.  If you have to use `@console` to get any output,
> you may have some issue with your kernel config.

**Example:**

    tty [12345] @console noclear vt220

On really bare bones systems, or for board bringup, Finit can give you a
shell prompt as soon as [bootstrap][] is done, without opening any
device node:

    tty [12345789] notty

This should of course not be enabled on production systems.  Because it
may give a user root access without having to log in.  However, for
board bringup and system debugging it can come in handy.

One can also use the `service` stanza to start a stand-alone shell:

    service [12345] /bin/sh -l


### Non-privileged Services

Every `run`, `task`, or `service` can also list the privileges the
`/path/to/cmd` should be executed with.  Prefix the command with
`@USR[:GRP]`, group is optional, like this:

    run [2345] @joe:users logger "Hello world"

For multiple instances of the same command, e.g. a DHCP client or
multiple web servers, add `:ID` somewhere between the `run`, `task`,
`service` keyword and the command, like this:

    service :80  [2345] httpd -f -h /http -p 80   -- Web server
    service :8080[2345] httpd -f -h /http -p 8080 -- Old web server

Without the `:ID` to the service the latter will overwrite the former
and only the old web server would be started and supervised.


### Redirecting Output

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


Rescue Mode
-----------

Finit supports a rescue mode which is activated by the `rescue` option
on the kernel command line.  See [cmdline docs](cmdline.md) for how to
activate it.

The rescue mode comes in two flavors; *traditional* and *fallback*.

> **Note:** in this mode `initctl` will not work.  Use the `-f` flag to
> force `reboot`, `shutdown`, or `poweroff`.


### Traditional

This is what most users expect.  A very early maintenance login prompt,
served by the bundled `/libexec/finit/sulogin` program, or the standard
`sulogin` from util-linux or BusyBox is searched for in the UNIX default
`$PATH`.  If a successful login is made, or the user exits (Ctrl-D), the
rescue mode is ended and the system boots up normally.

> **Note:** if the user (UID 0 and GID 0) does not have a password, or
> __the account is locked__, the user is presented with a password-less
> `"Press enter to enter maintenance mode."`, prompt which opens up a
> root shell.


### Fallback

If no `sulogin` program is found, Finit tries to bring up as much of its
own functionality as possible, yet limiting many aspects, meaning; no
network, no `fsck` of file systems in `/etc/fstab`, no `/etc/rc.local`,
no `runparts`, and most plugins are skipped (except those that provide
functionality for the condition subsystem).

Instead of reading `/etc/finit.conf` et al, system configuration is read
from `/lib/finit/rescue.conf`, which can be freely modified by the
system administrator.

The bundled default `rescue.conf` contains nothing more than:

    runlevel 1
    tty [12345] rescue

The `tty` has the `rescue` option set, which works similar to the board
bring-up tty option `notty`.  The major difference being that `sulogin`
is started to query for root/admin password.  If `sulogin` is not found,
`rescue` behaves like `notty` and gives a plain root shell prompt.

If Finit cannot find `/lib/finit/rescue.conf` it defaults to:

    tty [12345] rescue

There is no way to exit the *fallback* rescue mode.


Limitations
-----------

As of Finit v4 there are no limitations to where `.conf` settings can be
placed.  Except for the system/global `rlimit` and `cgroup` top-level
group declarations, which can only be set from `/etc/finit.conf`, since
it is the first `.conf` file Finit reads.

Originally, `/etc/finit.conf` was the only way to set up a Finit system.
Today it is mainly used for bootstrap settings like system hostname,
early module loading for watchdogd, network bringup and system shutdown.
These can now also be set in any `.conf` file in `/etc/finit.d`.

There is, however, nothing preventing you from having all configuration
settings in `/etc/finit.conf`.

> **Note:** The `/etc/finit.d` directory was previously the default
>          Finit [runparts](#run-parts-scripts) directory.  Finit >=v4.0
>          no longer has a default `runparts` directory, make sure to
>          update your setup, or the finit configuration, accordingly.


Watchdog
--------

When built `--with-watchdog` a separate service is built and installed
in `/libexec/finit/watchdogd`.  If this exists at runtime, and the WDT
device node exists, Finit will start it and treat it as the elected
watchdog service to delegate its reboot to.  See [bootstrap][] for
details.  This delegation is to ensure that the system is rebooted by a
hardware watchdog timer -- on many embedded systems this is crucial to
ensure all circuits on the board are properly reset for the next boot,
in effect ensuring the system works the same after both a power-on and
reboot event.

The delegation is performed at the very last steps of system shutdown,
if reboot has been selected and an elected watchdog is known, first a
`SIGPWR` is sent to advise watchdogd of the pending reboot.  Then, when
the necessary steps of preparing the system for shutdown (umount etc.)
are completed, Finit sends `SIGTERM` to watchdogd and puts itself in a
10 sec timeout loop waiting for the WDT to reset the board.  If a reset
is not done before the timeout, Finit falls back to`reboot(RB_AUTOBOOT)`
which tells the kernel to do the reboot.

An external watchdog service can also be used.  The more advanced cousin
[watchdogd](https://github.com/troglobit/watchdogd/) is the recommended
option here.  It can register itself with Finit using the same IPC as
`initctl`.  If the bundled watchdogd is running a hand-over takes place,
so it's safe to have both services installed on a system.  For the
hand-over to work it requires that the WDT driver supports the safe exit
functionality where `"V"` is written to the device before closing the
device descriptor.  If the kernel driver has been built without this,
the only option is to remove `/libexec/finit/watchdogd` or build without
it at configure time.

[bootstrap]: bootstrap.md


keventd
-------

The kernel event daemon bundled with Finit is a simple uevent monitor
for `/sys/class/power_supply`.  It provides the `sys/pwr/ac` condition,
which can be useful to prevent power hungry services like anacron to run
when a laptop is only running on battery, for instance.

Since keventd is not an integral part of Finit yet it is not enabled by
default.  Enable it using `./configuure --with-keventd`.  The bundled
contrib build scripts for Debian, Alpine, and Void have this enabled.

This daemon is planned to be extended with monitoring of other uevents,
patches and ideas are welcome in the issue tracker.
