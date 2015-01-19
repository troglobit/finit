README
======
[![Build Status](https://travis-ci.org/troglobit/finit.png?branch=master)](https://travis-ci.org/troglobit/finit)
[![Coverity Scan Status](https://scan.coverity.com/projects/3545/badge.svg)](https://scan.coverity.com/projects/3545)

Init is the first process to run once a UNIX kernel has booted, it
always has PID 1 and is responsible for starting up the rest of the
system.  Finit is a very small plugin based init with built in
[process supervision](https://en.wikipedia.org/wiki/Process_supervision)
similar to that of D.J. Bernstein's
[daemontools](http://cr.yp.to/daemontools.html) and Gerrit Pape's
[runit](http://smarden.org/runit/).  Its focus is on small and embedded
GNU/Linux systems, although fully functional on standard server and
desktop installations.

Finit is fast because it is "scripted" with C and starts services in
parallel.  Services are supervised and automatically restarted if they
fail.  Services can have callbacks that are called before a service is
(re)started.  For instance, before attempting to start a heavily
resource intensive service like IPsec or OpenVPN, a callback can check
if the outbound interface is up and has an IP address, or just check if
the service is disabled -- much like what a SysV init start script
usually does.

To extend finit with new functionality, hooks can be added at certain
points in the boot process that execute in sequence, which is useful for
certain satisfying preconditions or for synchronization purposes.  One
can also create plugins to functionality to suit your own needs, the
initctl compatibility plugin that comes with finit is one such plugin.

Finit is not only fast, it's arguably one of the easiest to get started
with.  A complete system can be booted with one simple configuration
file, `/etc/finit.conf`, see below for syntax.


Features
--------

**Process Supervision**

Start, monitor and restart services if they fail.

**Runlevels**

Finit supports standard runlevels if you want, but you don't need them
for simple installations.

**Plugins**

Extend and modify finit behavior.  See examples in plugins/ directory.
Plugin capabilities:

* Service callbacks -- modify service arguments, run/restart/stop
* Task/Run callbacks -- a one-shot commands, executed in sequence
* Hooks -- hook into the boot at predefined points to extend finit
* I/O -- listen to external events and control finit behavior/services

Extensions and functionality not purely related to what an /sbin/init
needs to start a system are available as a set of plugins that either
hook into the boot process or respond to various I/O.


/etc/finit.conf
---------------

Contrary to most other script based init alternatives
([SysV init](https://en.wikipedia.org/wiki/Init),
[upstart](http://upstart.ubuntu.com/),
[systemd](http://www.freedesktop.org/wiki/Software/systemd/),
[OpenRC](http://www.gentoo.org/proj/en/base/openrc/) and the likes)
finit reads its configuration from `/etc/finit.conf`.  Syntax:

* `check <DEV>`

  Run fsck on a file system before mounting it

* `module <MODULE>`

  Load a kernel module, with optional arguments

* `network <PATH>`

  Script or program to bring up networking, with optional arguments

* `runlevel <N>`

  N is the runlevel number 1-9, where 6 is reserved for reboot.

  Default is 2.

* `run [LVLS] /path/to/cmd ARGS -- Optional description`

  One-shot command to run in sequence when entering a runlevel, with
  optional arguments and description.  This command is guaranteed to be
  completed before running the next command.

* `task [LVLS] /path/to/cmd ARGS -- Optional description`

  One-shot like 'run', but starts in parallel with the next command

* `service [LVLS] /path/to/daemon ARGS -- Optional description`

  Service, or daemon, to be monitored and automatically restarted if it
  exits prematurely.  Please note that you often need to provide a
  --foreground or --no-background argument to most daemons to prevent
  them from forking off to the background.

* `runparts <PATH>`

  Call run-parts(8) on a directory other than default ``/etc/finit.d``

* `include <CONF>`

  Include another configuration file, ``/etc/finit.d``, or runparts path
  is prepended to file if the file is not found or an absolute path is
  not given

* `tty [LVLS] <DEV | /bin/sh>`

  Start a getty on the given TTY device, in the given runlevels.  When
  no tty setting is given in `finit.conf`, or if `/bin/sh` is given as
  argument instead of a device path, a single shell is started on the
  default console.  Useful for really bare-bones systems

  See `finit.h` for the `#define GETTY` that is called, along with the
  default baud rate.

* `console <DEV>`

  Some embedded systems have a dedicated console port.  This command
  tells finit to not start getty, but instead print a friendly message
  and wait for the user to activate the console with a key press before
  starting getty.

When running `make install` no default `/etc/finit.conf` will be
provided since the system requirements differ too much.  Try out the
Debian 6.0 example `/usr/share/doc/finit/finit.conf` configuration that
is capable of service monitoring SSH, sysklogd, gdm and a console getty!


/etc/finit.d
------------

At the end of the boot, when networking and all services are up, finit
calls its built-in run-parts(8) on the `/etc/finit.d/` directory, if it
exists.  Similar to how the `/ec/rc.local` file works in most other init
daemons, only finit runs a directory of scripts.  This replaces the
earlier support for a `/usr/sbin/services.sh` script in the original
finit.


Runlevels
---------

Basic support for [runlevels](http://en.wikipedia.org/wiki/Runlevel) is
included in Finit from v1.8.  By default all services, tasks, run
commands and TTYs listed without a set of runlevels get a default set
`[234]` assigned.  The default runlevel after boot is 2.

To specify an allowed set of runlevels for a `service`, `run` command, `task`,
or `tty`, add `[NNN]` to it in your `finit.conf`, like this:

    run     [S]      /etc/init.d/acpid start -- Starting ACPI Daemon
    task    [S]      /etc/init.d/kbd start   -- Preparing console
    service [S12345] /sbin/klogd -n -x       -- Kernel log daemon
    tty     [12345]  /dev/tty1
    tty     [2]      /dev/tty2
    tty     [2]      /dev/tty3
    tty     [2]      /dev/tty4
    tty     [2]      /dev/tty5
    tty     [2]      /dev/tty6

In this example acpid is started once at bootstrap using a conventional
SysV init script.  Here the run command was used, meaning the following
task command is not run until the init script has fully completed.  Then
the keyboard setup script is called in parallel with spawning klogd as a
monitored service.

Tasks and services are started in parallel, while run commands are run
in the order listed and subsequent commands are not started until a run
command has completed.

Existing finit.conf files that lack runlevel setting will get a default
runlevel assigned, `[234]`.

Switching between runlevels can be done by calling init with a single
argument, e.g., `init 5` switches to runlevel 5.


Hooks, Callbacks & Plugins
--------------------------

Finit provides only the bare necessities for starting and supervising
processes, with an emphasis on *bare* -- for your convenience it does
however come with support for hooks, service callbacks and plugins that
can used to extend finit with.  For your convenience a set of *optional*
plugins are available:

* *alsa-utils.so*: Restore and save ALSA sound settings on
  startup/shutdown.

* *bootmisc.so*: Setup necessary files for UTMP, tracks logins at boot.

* *dbus.so*: Setup and start system message bus, D-Bus, at boot.

* *hwclock.so*: Restore and save system clock from/to RTC on
  startup/shutdown.

* *initctl.so*: Extends finit with a traditional initctl functionality.

* *resolvconf.so*: Setup necessary files for resolvconf at startup.

* *tty.so*: Watches /dev, using inotify, for new device nodes (TTY's) to
  start/stop getty consoles on them on demand.

* *urandom.so*: Setup random seed at startup.

* *x11-common.so*: Setup necessary files for X-Window.

Usually you want to hook into the boot process once, simple hook plugins
like `bootmisc.so` are great for that purpose.  They are called at each
hook point in the boot process, useful to insert some pre-bootstrap
mechanisms, like generating configuration files, restoring HW device
state, etc.  Available hook points are:

* `HOOK_ROOTFS_UP`: When `finit.conf` has been read and `/` has is
  mounted -- very early

* `HOOK_BASEFS_UP`: All of `/etc/fstab` is mounted, swap is available
  and default init signals are setup

* `HOOK_NETWORK_UP`: System bootstrap, runlevel S, has completed and
  networking is up (`lo` is up and the `network` script has run)

* `HOOK_SVC_UP`: All services in the active runlevel has been launched

* `HOOK_SYSTEM_UP`: All services *and* everything in `/etc/finit.d`
  has been launched

* `HOOK_SHUTDOWN`: Called at shutdown/reboot, right before all
  services are sent `SIGTERM`

Plugins like `initctl.so` and `tty.so` extend finit by acting on events,
they are called I/O plugins and are called from the finit main loop when
`poll()` detects an event.  See the source code for `plugins/*.c` for
more help and ideas.

Callback plugins are called by finit them right before a process is
started, or restarted if it exits.  The callback receive a pointer to
the `svc_t` (defined in `svc.h`) of the service, with all command line
parameters free to modify if needed.  All the callback needs to do is
respond with one of: `SVC_STOP (0)` tells finit to *not* start the
service, `SVC_START (1)` to start the service, or `SVC_RELOAD (2)` to
have finit signal the process with `SIGHUP`.


Rebooting and Halting
---------------------

Finit handles `SIGUSR1` and `SIGUSR2` for reboot and halt, and listens
to `/dev/initctl` so system reboot and halt commands should also work.
This latter functionality is implemented in the optional `initctl.so`
plugin.


Building
--------

The finit build system does not employ the GNU Configure and Build System,
instead standard makefiles are used. The user is encouraged to make source
code changes, using defines and conditionally building plugins instead to
alter the behavior of finit.

The following environment variables are checked by the makefiles and control
what is built and where resulting binaries are installed.

* `ROOTDIR=`: Top directory for building complete system, used in pretty
  printing.

* `VERSION=`: Defaults to the currently released version of finit,
  e.g., 1.3 but can be overridden by packagers using this variable to
  add a suffix or completely alter the version.

* `CFLAGS=`: Default `CFLAGS` are inherited from the environment.

* `CPPFLAGS=`: Default `CPPFLAGS` are inherited from the environment.

* `LDFLAGS=`: Default `LDFLAGS` are inherited from the environment.

* `LDLIBS=`: Default `LIBLIBS` are inherited from the environment.

* `prefix=`: Base prefix path for all files, except `sbinbdir` and
  `sysconfdir`.  Used in concert with the `DESTDIR` variable.

  Defaults to `/usr`.

* `sbindir=`: Path to where resulting binaries should install to. Used
  in concert with the `DESTDIR` variable.

  Defaults to `/sbin`.

* `sysconfdir=`: Path to where finit configuration files should install
  to.  Used in concert with the `DESTDIR` variable.

  Defaults to `/etc`, but is currently unused.

* `PLUGINS=`: List of stock finit plugins to build and install.

* `plugindir=`: Absolute path to where finit should look for dynamically
  loadable plugins at runtime.  At installation prepended by `DESTDIR`
  and `prefix`.

  Defaults to `/lib/finit/plugins`.

* `DESTDIR=`: Used by packagers and distributions when building a
  relocatable bundle of files.  Always prepended to the `prefix`
  destination directory.

**Example**

    $ tar xfJ finit-1.3.tar.xz
    $ PLUGINS="initctl.so hwclock.so" DESTDIR=/tmp/finit/dst \
      make -C finit-1.3/ clean install
    make: Entering directory `/home/troglobit/finit-1.3'
      CC      finit.o
      CC      conf.o
      CC      helpers.o
      CC      sig.o
      CC      svc.o
      CC      plugin.o
      CC      strlcpy.o
      LINK    finit
      CC      plugins/initctl.o
      PLUGIN  plugins/initctl.so
      CC      plugins/hwclock.o
      PLUGIN  plugins/hwclock.so
      INSTALL /tmp/finit/dst/sbin/finit
      INSTALL /tmp/finit/dst/lib/finit/plugins/initctl.so
      INSTALL /tmp/finit/dst/lib/finit/plugins/hwclock.so
    make: Leaving directory `/home/troglobit/finit-1.3'

In this example the
[finit-1.3.tar.xz](ftp://troglobit.com/finit/finit-1.3.tar.xz)
archive is unpacked to the user's home directory, built and
installed to a temporary staging directory.  The environment
variables `DESTDIR` and `PLUGINS` are changed to suit this
particular build.


Running
-------

The default install does not setup finit as the system default
`/sbin/init`, neither does it setup an initial `/etc/finit.conf`.

It is assumed that users of finit are competent enough to either setup
finit as their default `/sbin/init` or alter their respective Grub,
LOADLIN, LILO, U-Boot/Barebox or RedBoot boot loader configuration to
give the kernel the following extra command line:

    init=/sbin/finit


Debugging
---------

Add `finit_debug`, or `--debug`, to the kernel command line to
enable trace messages.  A console getty is always started, see the file
`finit.h` for more useful compile-time tweaks:

    init=/sbin/finit --debug


Credits & Contact
-----------------

This is the continuation of the [original finit](http://helllabs.org/finit/)
by [Claudio Matsuoka](https://github.com/cmatsuoka), which in turn was
reverse engineered from syscalls of the
[EeePC fastinit](http://wiki.eeeuser.com/boot_process:the_boot_process)
-- "gaps filled with frog DNA ...".

Finit is currently being developed and maintained by
[Joachim Nilsson](http://troglobit.com) at
[GitHub](http://github.com/troglobit/finit).  Please file bug reports, clone it,
or send pull requests for bug fixes and proposed extensions.

