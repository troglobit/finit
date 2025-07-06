Finit Configuration
===================

* [Introduction](#introduction)
  * [Configuration Files](#configuration-files)
  * [Runlevels](#runlevels)
  * [Managing Services](#managing-services)
  * [Environment Variables](#environment-variables)
* [Service Environment](#service-environment)
* [Service Synchronization](#service-synchronization)
* [Service Wrapper Scripts](#service-wrapper-scripts)
* [Templating](#templating)
* [Cgroups](#cgroups)
* [Configuration File Syntax](#configuration-file-syntax)
  * [Hostname](#hostname)
  * [Kernel Modules](#kernel-modules)
  * [Networking](#networking)
  * [Alternate rcS.d/](#alternate-rcs-d-)
  * [Resource Limits](#resource-limits)
  * [Runlevels](#runlevels)
  * [One-shot Commands (sequence)](#one-shot-commands-sequence)
  * [One-shot Commands (parallel)](#one-shot-commands-parallel)
  * [SysV Init Scripts](#sysv-init-scripts)
  * [Services](#services)
  * [Run-parts Scripts](#run-parts-scripts)
  * [Including Finit Configs](#including-finit-configs)
  * [General Logging](#general-logging)
  * [TTYs and Consoles](#ttys-and-consoles)
  * [Non-privileged Services](#non-privileged-services)
  * [Redirecting Output](#redirecting-output)
* [SysV Init Compatibility](#sysv-init-compatibility)
* [Rescue Mode](#rescue-mode)
* [Limitations](#limitations)
  * [/etc/finit.conf](#etcfinitconf)
  * [/etc/finit.d](#etcfinitd)
* [Watchdog](#watchdog)
* [keventd](#keventd)


Introduction
------------

Finit is a process starter and service monitor designed to run as PID 1
on Linux.  It consists of [plugins](plugins.md) and configuration files.
Plugins start at [hook points](plugins.md#hooks) and can run various set
up, or install event handlers that later provide runtime services, e.g.,
PID file monitoring, or [conditions](conditions.md).

> [!TIP]
> See [SysV Init Compatibility](#sysv-init-compatibility) for help to
> quickly get going with an existing SysV or BusyBox init setup.


### Configuration Files

Originally Finit was configured using a single file, `/etc/finit.conf`,
and although still possible to use a single configuration file, today
the following layout is recommended:

    /
    |- etc/
    |  |- finit.d/
    |  |   |- available/
    |  |   |  `- my-service.conf
    |  :   |- enabled/
    |  :   |  `- my-service.conf -> ../available/my-service.conf
    |  :   :
    |  :   |- static-service.conf
    |  :   `- another-static.conf
    |  :
    |  `- finit.conf
    |- lib
    |   `- finit/
    |       `- system/
    |           |- 10-hotplug.conf
    |           `- ...
    `- run/
        `- finit/
            `- system/
                |- dbus.conf
                |- keventd.conf
                |- runparts.conf
                |- watchdogd.conf
                `- ...

Configuration files in `/etc` are provided by the user, or projects like
[finit-skel](https://github.com/troglobit/finit-skel) and extended by
the user.

The files in `/lib/finit/system/*.conf` are system critical services and
setup provided by Finit, e.g. udev/mdev that must run very early at system
bootstrap.  This system directory was introduced in Finit v4.4 to replace
the hard-coded services provided by plugins before.  All .conf files in this
directory be either replaced by a system administrator or overridden by a
file with the same name in `/etc/finit.d/`.

The files in `/run/finit/system/*.conf` are created by plugins and Finit
bundled services like runparts, the [watchdog](#watchdog), and `keventd`
if they are enabled.  Like `/lib/finit/system/*.conf`, these files can
 be overridden by file with the same name in `/etc/finit.d/`.

Services in the `available/` and `enabled/` sub-directories are called
dynamic services, in contrast to static services -- the only difference
being where they are installed and if the `initctl` tool can manage them
with the `enable` and `disable` commands.  An administrator can always
create files and symlinks manually.

At bootstrap, and `initctl reload`, all .conf files are read, starting
with `finit.conf`, then `/lib/finit/system/*.conf`, `finit.d/*.conf`,
and finally all `finit.d/enabled/*.conf` files.  Each directory is a
unique group, where files within each group are sorted alphabetically.

**Example:**

    /lib/finit/system/10-hotplug.conf
    /lib/finit/system/90-testserv.conf
    /run/finit/system/dbus.conf
    /run/finit/system/runparts.conf
    /etc/finit.d/10-abc.conf
    /etc/finit.d/20-abc.conf
    /etc/finit.d/enabled/1-aaa.conf
    /etc/finit.d/enabled/1-abc.conf

The resulting combined configuration is read line by line, each `run`,
`task`, and `service` added to an ordered list that ensures they are
started in the same order.  This is important because of the blocking
properties of the `run` statement.  For an example on the relation of
`service` and `run` statements, and dependency handling between them,
see [Conditional Loading](#conditional-loading), below.

> [!NOTE]
> The names `finit.conf` and `finit.d/` are only defaults.  They can be
> changed at compile-time with two `configure` options:
> `--with-config=/etc/foo.conf` and `--with-rcsd=/var/foo.d`.
>
> They can also be overridden from the [kernel command line](cmdline.md)
> using: `-- finit.config=/etc/bar.conf` and in that file use the
> top-level configuration directive `rcsd /path/to/finit.d`.


### Filesystem Layout

Finit is most comfortable with a traditional style Linux filesystem
layout, as specified in the [FHS][]:

    /.
     |- bin/
     |- dev/          # Mounted automatically if devtmpfs is available
     |   |- pts/      # Mounted automatically by Finit if it exists
     |   `- shm/      # Mounted automatically by Finit if it exists
     |- etc/
     |   |- finit.d/
     |   |   |- available/
     |   |   `- enabled/
     |    `- finit.conf
     |- home/
     |- lib/
     |- libexec/
     |- mnt/
     |- proc/         # Mounted automatically by Finit if it exists
     |- root/
     |- run/          # Mounted automatically by Finit if it exists
     |   `- lock/     # Created automatically if Finit mounts /run
     |- sbin/
     |- sys/          # Mounted automatically by Finit if it exists
     |- tmp/          # Mounted automatically by Finit if it exists
     |- usr/
     `- var/
         |- cache/
         |- db/
         |- lib/
         |   `- misc/
         |- lock/
         |- log/
         |- run -> ../run
         |- spool/
         `- tmp/

Finit starts by mounting the critical file systems `/dev`, `/proc/`, and
`/sys`, unless they are already mounted.  When all plugins and other,
core Finit functions, have been set up, all relevant filesystems (where
`PASS > 0`) are checked and mounted from the selected `fstab`, either
the default `/etc/fstab`, or any custom one selected from the command
line, or at build time.

To provide a smooth ride, file system not listed in the given `fstab`,
e.g. `/tmp` and `/run`, are automatically mounted by Finit, as listed
above, provided their respective mount point exists.

With all filesystems mounted, Finit calls `swapon`.

> [!TIP]
> To see what happens when all filesystems are mounted, have a look at
> the [`bootmisc.so` plugin](plugins.md).

At shutdown, and after having stopped all services and other lingering
processes have been killed, filesystems are unmounted in the reverse
order, and `swapoff` is called.

[FHS]: https://refspecs.linuxfoundation.org/FHS_3.0/fhs/index.html

### Runlevels

Finit supports runlevels, but unlike other init systems runlevels are
declared per service/run/task/sysv command.  When booting up a system
Finit pass through three phases:

 1. Setting up the console, parsing any command line options, and other
    housekeeping tasks like mounting all filesystems, and calling `fsck`
 2. Starting all run/task/services in runlevel S, then waiting for all
    services to have started, and all run/tasks to have completed
 3. Go to runlevel 2, or whatever the user has set in the configuration

Available runlevels:

 - `  S`: bootStrap
 - `  1`: Single user mode
 - `2-5`: traditional multi-user mode
 - `  6`: reboot
 - `7-9`: multi-user mode (extra)
 - `  0`: shutdown

Runlevel S (bootStrap), is for tasks supposed to run once at boot, and
services like `syslogd`, which need to start early and run throughout
the lifetime of your system.

Example:

    task [S] /lib/console-setup/console-setup.sh
    service [S12345] env:-/etc/default/rsyslog rsyslogd -n $RSYSLOGD_ARGS

When bootstrap has completed, Finit moves to runlevel 2.  This can be
changed in `/etc/finit.conf` using the `runlevel N` directive, or by a
script running in runlevel S that calls, e.g., `initctl runlevel 9`.
The latter is useful if startup scripts detect problems outside of
Finit's control, e.g., critical services/devices missing or hardware
problems.

Each runlevel must be allowed to "complete".  Meaning, all services in
runlevel S must have started and all run/tasks have been started and
collected (exited).  Finit waits 120 seconds for all run/tasks in S to
complete before proceeding to 2.

Finit first stops everything that is not allowed to run in 2, and then
brings up networking.  Networking is expected to be available in all
runlevels except: S, 1 (single user level), 6, and 0.  Networking is
enabled either by the `network script` directive, or if you have an
`/etc/network/interfaces` file, Finit calls `ifup -a` -- at the very
least the loopback interface is brought up.

> [!NOTE]
> When moving from runlevel S to 2, all run/task/services that were
> constrained to runlevel S only are dropped from bookkeeping.  So when
> reaching the prompt, `initctl` will not show these run/tasks.  This is
> a safety mechanism to prevent bootstrap-only tasks from accidentally
> being run again.  E.g., `console-setup.sh` above.


### Managing Services

Using `initctl disable my-service` the symlink (above) is removed and
the service is queued for removal.  Several changes can be made to the
system, but it is not until `initctl reload` is called that the changes
are activated.

To add a new static service, drop a `.conf` file in `/etc/finit.d/` and
run `initctl reload`.  (It is also possible to `SIGHUP` PID 1, or call
`finit q`, but that has been deprecated with the `initctl` tool).  Finit
monitors all known active `.conf` files, so if you want to force a
restart of any service you can touch its corresponding `.conf` file in
`/etc/finit.d` and call `initctl reload`.  Finit handles all conditions
and dependencies between services automatically, see below section on
[Service Synchronization](#service-synchronization) for more details.

On `initctl reload` the following is checked for all services:

- If a service's `.conf` file has been removed, or its conditions are no
  longer satisfied, the service is stopped.
- If the file is modified, or a service it depends on has been reloaded,
  the service is reloaded (stopped and started).
- If a new service is added it is automatically started — respecting
  runlevels and return values from any callbacks.

For more info on the different states of a service, see the separate
document [Finit Services](service.md).


### Environment Variables

In Finit v4.3 support for setting environment variables in `finit.conf`,
and any `*.conf`, was added.  It is worth noting that these are global
and *shared with all* services -- the only way to have service-local
environment is detailed in the next section.

The syntax for global environment variables is straight forward.  In
Finit v4.4 the `set` keyword was added for completeness, but the old
syntax (without the `set ` prefix) is still honored:

    set foo=bar
    set baz="qux"

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

> [!NOTE]
> The leading `-` in `env:` determines if Finit should treat a missing
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

As of Finit v4.4 partial support for systemd and s6 style readiness
notification is available, and the native PID file mode of operation is,
as of Finit v4.6 optional, by default it is still enabled, but this can
be changed in `finit.conf`:

    readiness none

This will be made the default in Finit 5.0.  In this mode of operation,
every service needs to explicitly declare their readiness notification,
like this:

    service notify:pid     watchdogd
    service notify:systemd foo
    service notify:s6      bar
    service notify:none    qux

The `notify:none` syntax is for completeness in systems which run in
`readiness pid` mode (default).  Services declared with `notify:none`
will transition to ready as soon as Finit has started them, e.g.,
`service/qux/ready`.

To synchronize two services the following condition can be used:

    service notify:pid                 watchdogd
    service <service/watchdogd/ready>  stress-ng --cpu 8

For details on the syntax and options, see below.

> [!NOTE]
> On `initctl reload` conditions are set in "flux", while figuring out
> which services to stop, start or restart.  Services that need to be
> restarted have their `ready` condition removed before Finit issue a
> SIGHUP (if they support that), or stop-starting them.  A daemon is
> expected to reassert its readiness, e.g. systemd style daemons to
> write `READY=1\n`.
>
> However, the s6 notify mode does not support this because in s6 you
> are expected to close your notify descriptor after having written
> `\n`.  This means s6 style daemons currently must be stop-started.
> (Declare the service with `<!>` in its condition statement.)
>
> For default, PID file style readiness notification, daemons are
> expected to either create their PID files, or touch it using
> `utimensat()` to reassert readiness.  Triggering both the `<pid/>`
> and `<.../ready>` conditions.


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

> [!NOTE]
> The example sets `<!>` to denote that it doesn't support `SIGHUP`.
> That way Finit will stop/start the service instead of sending SIGHUP
> at restart/reload events.


Templating
----------

Finit comes with rudimentary support for templating, similar to that of
systemd.  Best illustrated with an example:

    $ initctl show avahi-autoipd@
    service :%i avahi-autoipd --syslog %i -- ZeroConf for %i

To enable ZeroConf for, e.g., eth0, use

    $ initctl enable avahi-autoipd@eth0.conf

The enabled symlink will be set up to avahi-autoipd@.conf and every in‐
stance of %i will in the instantiated directive be replaced with eth0.
Inspect the result with:

    $ initctl status avahi-autoipd:eth0


Cgroups
-------

There are three major cgroup configuration directives:

 1. Global top-level group: `init`, `system`, `user`, or a custom group
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

A daemon using `SCHED_RR` currently need to run outside the default cgroups.

    service [...] <...> cgroup.root /path/to/daemon arg -- Real-Time process

> [!NOTE]
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
following directives.  Some directives have restrictions, e.g., only
available at bootstrap, runlevel `S`, see [Limitations](#limitations)
below for details.


### Hostname

**Syntax:** `host <NAME>`, or `hostname <NAME>`

Set system hostname to NAME, unless `/etc/hostname` exists in which case
the contents of that file is used.

Deprecated.  We recommend using `/etc/hostname` instead.

> [!NOTE]
> Only read and executed in runlevel S (bootstrap).


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

> [!NOTE]
> Only read and executed in runlevel S (bootstrap).


### Networking

**Syntax:** `network <PATH>`

Script or program to bring up networking, with optional arguments.

Deprecated.  We recommend using dedicated task/run stanzas per runlevel,
or `/etc/network/interfaces` if you have a system with `ifupdown`, like
Debian, Ubuntu, Linux Mint, or an embedded BusyBox system.

> [!NOTE]
> Only read and executed in runlevel S (bootstrap).


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

> [!NOTE]
> This directive is only available from the top-level bootstrap .conf
> file, usually `/etc/finit.conf`.

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

The system runlevel to go to after bootstrap (S) has completed.  `N` is
the runlevel number 0-9, where 6 is reserved for reboot and 0 for halt.
Completed in this context means all services have been started and all
run/tasks have been started and collected.

It is recommended to keep runlevel 1 as single-user mode, because
Finit disables networking in this mode.

*Default:* 2

> [!NOTE]
> Only read and executed in runlevel S (bootstrap).


### One-shot Commands (sequence)

**Syntax:** `run [LVLS] <COND> /path/to/cmd ARGS -- Optional description`

> `<COND>` is described in the [Services](#services) section.

One-shot command to run in sequence when entering a runlevel, with
optional arguments and description.  `run` commands are guaranteed to be
completed before running the next command.  Useful when serialization is
required.

> [!WARNING]
> Try to avoid the `run` command.  It blocks much of the functionality
> in Finit, like (re)starting other (perhaps crashing) services while a
> `run` task is executing.  Use other synchronization mechanisms
> instead, like conditions.

Incomplete list of unsupported `initctl` commands in `run` tasks:

 - `initctl runlevel N`, setting runlevel
 - `initctl reboot`
 - `initctl halt`
 - `initctl poweroff`
 - `initctl suspend`

To prevent `initctl` from calling Finit when enabling and disabling
services from inside a `run` task, use the `--force` option.  See
also the `--quiet` and `--batch` options.


### One-shot Commands (parallel)

**Syntax:** `task [LVLS] <COND> /path/to/cmd ARGS -- Optional description`

> `<COND>` is described in the [Services](#services) section.

One-shot like 'run', but starts in parallel with the next command.
  
Both `run` and `task` commands are run in a shell, so basic pipes and
redirects can be used:

    task [s] echo "foo" | cat >/tmp/bar

Please note, `;`, `&&`, `||`, and similar are *not supported*.  Any
non-trivial constructs are better placed in a separate shell script.


### SysV Init Scripts

**Syntax:** `sysv [LVLS] <COND> /path/to/init-script -- Optional description`

> `<COND>` is described in the [Services](#services) section.

Similar to `task` is the `sysv` stanza, which can be used to call SysV
style scripts.  The primary intention for this command is to be able to
reuse much of existing setup and init scripts in Linux distributions.
  
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

> [!TIP]
> See also [SysV Init Compatibility](#sysv-init-compatibility).


### Services

**Syntax:** `service [LVLS] <COND> /path/to/daemon ARGS -- Optional description`

Service, or daemon, to be monitored and automatically restarted if it
exits prematurely.  Finit tries to restart services that die, by default
10 times before giving up and marking them as *crashed*.  After which
they have to be manually restarted with `initctl restart NAME`.  The
limits controlling this are configurable, see the options below.

> [!TIP]
> To allow endless restarts, see below option `respawn`
  
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

If multiple instances of a service, with the same `name`, exist.  Set
the identifier `:ID` to prevent Finit from replacing previous instances:

    service :eth1 ssdpd eth1 -- Windows discovery on eth1
    service :eth2 ssdpd eth2 -- Windows discovery on eth2

If you have conflicting services and want to prevent them from starting,
use the `conflict:` argument:

    service [S12345789] udevd -- Device event management daemon
	run [S] conflict:udevd mdev -s -- Populating device tree

Multiple conflicting services can be separated using `,`:

    service :1 abc
    service :2 abc
	service conflict:abc:1,abc:2 cde

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
    has *crashed*, if this option is set the system is rebooted
  * `oncrash:script` -- similarly, but instead of rebooting, call the
    `post:script` action with exit code `crashed`, see below
  * `reload:script args` -- some services do not support `SIGHUP` but
    may have other ways to update the configuration of a running daemon.
    When `reload:script` is defined it is preferred over `SIGHUP`

> [!CAUTION]
> The `reload:script` is called in as PID 1, without any timeout!
> Meaning, it is up to you to ensure this script is not blocking
> for seconds at a time or never terminates.

When stopping a service (run/task/sysv/service), either manually or when
moving to another runlevel, Finit starts by sending `SIGTERM`, to allow
the process to shut down gracefully.  However, if the process has not
been collected within 3 seconds, Finit will send `SIGKILL`.  To stop the
process using a different signal than `SIGTERM`, use `halt:SIGNAL`,
e.g., `halt:SIGPWR`.  To change the delay between the stop signal and
KILL, use the option `kill:<1-60>`, e.g., `kill:10` to wait 10 seconds
before sending `SIGKILL`.

Services, including the `sysv` variant, support pre/post/ready and
cleanup scripts:

  * `pre:[0-3600,]script` -- called before the sysv/service is stated
  * `post:[0-3600,]script` -- called after the sysv/service has stopped
  * `ready:[0-3600,]script` -- called when the sysv/service is ready
  * `cleanup:[0-3600,]script` -- called when run/task/sysv/service is removed

The optional number (0-3600) is the timeout before Finit kills the
script, it defaults to the kill delay value and can be disabled by
setting it to zero.  These scripts run as the same `@USER:GROUP` as the
service itself, with any `env:file` sourced.  The scripts are executed
from the `$HOME` of the given user.  The scripts are not called with any
argument, but get a set of environment variables:

  * `SERVICE_IDENT=foo:1`
  * `SERVICE_NAME=foo`
  * `SERVICE_ID=1`

The `post:script` is called with an additional set of environment
variables.  Yes, the text is correct, the naming was an accident:

 - `EXIT_CODE=[exited,signal,crashed]`: normal exit, signaled, or
   crashed
 - `EXIT_STATUS=[num,SIGNAME]`: set to one of exit status code from
   the program, if it exited normally, or the signal name (`HUP`,
   `TERM`, etc.) if it exited due to signal

When a run/task/sys/service is removed (disable + reload) it is first
stopped and then removed from the runlevel.  The `post:script` always
runs when the process has stopped, and the `cleanup:script` runs when
the the stanza has been removed from the runlevel.

> [!IMPORTANT]
> These script actions are intended for setup, cleanup, and readiness
> notification.  It is up to the user to ensure the scripts terminate.


#### Conditional Loading

Finit support conditional loading of stanzas.  The following example is
take from the `system/hotplug.conf` file in the Finit distribution.
Here we only show a simplified subset.

Starting with the `nowarn` option.

    service nowarn name:udevd pid:udevd /lib/systemd/systemd-udevd
    service nowarn name:udevd pid:udevd udevd

When loading the .conf file Finit looks for `/lib/systemd/systemd-udevd`
if that is not found Finit automatically logs a warning.  The `nowarn`
option disables this warning so that the second line can be evaluated,
which also provides a service named `udevd`.

    run nowarn if:udevd <pid/udevd> :1 udevadm settle -t 0

This line is only loaded if we know of a service named `udevd`.  Again,
we do not warn if `udevadm` is not found, Execution will also stop here
until the PID condition is asserted, i.e., Finit detecting udevd has
started.

    run nowarn conflict:udevd [S] mdev -s -- Populating device tree

If `udevd` is not available, we try to run `mdev`, but if that is not
found, again we do not warn.

Conditional loading statements can also be negated, so the previous stanza
can also be written as:

    run nowarn if:!udevd [S] mdev -s -- Populating device tree

The reason for using `confict` in this example is that a conflict can be
resolved.  Stanzas marked with `conflict:foo` are rechecked at runtime.

#### Conditional Execution

Similar to conditional loading of stanzas there is conditional runtime
execution.  This can be confusing at first, since Finit already has a
condition subsystem, but this is more akin to the qualification to a
runlevel.  E.g., a `task [123]` is qualified to run only in runlevel 1,
2, and 3.  It is not considered for other runlevels.

Conditional execution qualify a run/task/service based on a condition.
Consider this (simplified) example from the Infix operating system:

    run [S]                       name:startup <pid/sysrepo> confd -b --load startup-config
    run [S] if:<usr/fail-startup> name:failure <pid/sysrepo> confd    --load failure-config

The two run statements reside in the same .conf file so Finit runs them
in true sequence.  If loading the file `startup-config` fails confd sets
the condition `usr/fail-startup`, thus allowing the next run statement
to load `failure-config`.

Notice critical the difference between `<pid/sysrepo>` condition and
`if:<usr/fail-startup>`.  The former is a condition for starting and the
latter is a condition to check if a run/task/service is qualified to
even be considered.

Conditional execution statements can also be negated, so provided the
file loaded did the opposite, i.e., set a condition on success, the
previous stanza can also be written as:

    run [S] if:<!usr/startup-ok> name:failure <pid/sysrepo> confd ...

### Run-parts Scripts

**Syntax:** `runparts [progress] [sysv] <DIR>`

Call [run-parts(8)][] on `DIR` to run start scripts.  All executable
files in the directory are called, in alphabetic order.  The scripts in
this directory are executed at the very end of runlevel `S`.

A common use-case for runparts scripts is to create and enable/disable
services, which Finit will then apply when changing runlevel from S to
whatever the next runlevel is set to be (default 2).  E.g., generate a
`/etc/chrony.conf` and call `initctl enable chronyd`.

**Options:**

 - `progress`: display the progress of each script being executed
 - `sysv`: run only SysV style scripts, i.e., `SNNfoo`, or `KNNbar`,
   where `NN` is a number (0-99).

If global debug mode is enabled, the `runparts` program is also called
with the debug flag.

**Limitations:**

Scripts called from `runparts`, or hook scripts (see below), are limited
in their interaction with Finit.  Like the standalone `run` stanza and
the `/etc/rc.local` shell script, Finit waits for their completion
before continuing.  None of them can issue commands to start, stop, or
restart other services.  Also, ensure all your services and programs
either terminate or start in the background or you will block Finit.

> [!NOTE]
> `runparts` scripts are only read and executed in runlevel S.  See
> [hook scripts](plugins.md#hooks) for other ways to run scripts at
> certain points during the complete lifetime of the system.

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
shell prompt as soon as bootstrap is done, without opening any device
node:

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


### Misc Settings

**Syntax:** `reboot-delay <0-60>`

Optional delay at reboot (or shutdown or halt) to allow kernel
filesystem threads to complete after calling `sync(2)` before
rebooting.  This applies primarily to filesystems that do not
have a reboot notifier implemented.  At the point of writing,
the only known filesystems affected are: ubifs, jffs2.

*Default:* 0 (disabled)

When enabled (non-zero), this delay runs after file systems have been
unmounted and the root filesystem has been remounted read-only, and
sync(2) has been called, twice.

> "On Linux, sync is only guaranteed to schedule the dirty blocks for
> writing; it can actually take a short time before all the blocks are
> finally written.


SysV Init Compatibility
-----------------------

It is not possible to run unmodified SysV init systems with Finit.  This
was never the intention and is not the strength of Finit.  However, it
comes with a few SysV Init compatibility features to ease the transition
from a serialized boot process.

### `runparts DIRECTORY`

For a directory with traditional start/stop scripts that should run, in
order, at bootstrap, Finit provides the `runparts` directive.  It runs
in runlevel S, at the very end of it (before calling `/etc/rc.local`)
making it perfect for most scenarios.

For syntax details, see [Run-parts Scripts](#run-parts-scripts) above.
Here is an example take from a Debian installation:

    runparts /etc/rc2.d

Files in these directories are usually named `SNNfoo` and `KNNfoo`,
which Finit knows about and automatically appends the correct argument:

    /bin/sh -c /etc/rc2.d/S01openbsd-inetd start

or

    /bin/sh -c /etc/rc0.d/K01openbsd-inetd stop

Files that do not match this pattern are started similarly but without
the extra command line argument.


### Start/Stop Scripts

For syntax details, see [SysV Init Scripts](#sysv-init-scripts), above.
Here follows an example taken from a Debian installation:

    sysv [2345] <pid/syslogd> /etc/init.d/openbsd-inetd -- OpenBSD inet daemon

The init script header could be parsed to extract `Default-Start:` and
other parameters for the `sysv` command to Finit.  There is currently no
way to detail a generic syslogd dependency in Finit, so `Should-Start:`
in the header must be mapped to the condition system in Finit using an
absolute reference, here we depend on the sysklogd project's syslogd.

### `/etc/rc.local`

One often requested feature, early on, was a way to run a site specific
script to set up, e.g., static routes or firewall rules.  A user can add
a `task` or `run` command in the Finit configuration for this, but for
compatibility reasons the more widely know `/etc/rc.local` is used if
it exists, and is executable.  It is called very late in the boot process
when the system has left runlevel S, stopped all old and started all new
services in the target runlevel (default 2).

> In Finit releases before v4.5 this script blocked Finit execution and
> made it as good as impossible to call `initctl` during that time.

### `init q`

When `/sbin/finit` is installed as `/sbin/init`, it is possible to use
`init q` to reload the configuration.  This is the same as calling
`initctl reload`.


Rescue Mode
-----------

Finit supports a rescue mode which is activated by the `rescue` option
on the kernel command line.  See [cmdline docs](cmdline.md) for how to
activate it.

This rescue mode can be disabled at configure time using:

    configure --without-rescue

The rescue mode comes in two flavors; *traditional* and *fallback*.

> [!NOTE]
> In this mode `initctl` will not work.  Use the `-f` flag to force
> `reboot`, `shutdown`, or `poweroff`.


### Traditional

This is what most users expect.  A very early maintenance login prompt,
served by the system `sulogin` program from util-linux, or BusyBox.  If
that is not found in `$PATH`, the bundled `/libexec/finit/sulogin`
program is used instead.  If a successful login is made, or if the user
exits (Ctrl-D), the rescue mode is ended and the system boots up
normally.

> [!WARNING]
> The bundled sulogin in Finit can at configure time be given another
> user than the default (root).  If the sulogin user does not have a
> password, or __the account is locked__, the user is presented with a
> prompt: `"Press enter to enter maintenance mode."`, which will open
> up a root shell *without prompting for password*!


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

> [!IMPORTANT]
> The default `rcsd`, i.e., `/etc/finit.d`, was previously the Finit
> [runparts](#run-parts-scripts) directory.  Finit >=v4.0 no longer has
> a default `runparts` directory, make sure to update your setup, or the
> finit configuration, accordingly.


Watchdog
--------

When built `--with-watchdog` a separate service is built and installed
in `/libexec/finit/watchdogd`.  If this exists at runtime, and the WDT
device node exists, Finit will start it and treat it as the elected
watchdog service to delegate its reboot to.  This delegation is to
ensure that the system is rebooted by a hardware watchdog timer -- on
many embedded systems this is crucial to ensure all circuits on the
board are properly reset for the next boot, in effect ensuring the
system works the same after both a power-on and reboot event.

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
