```
                      _______  __    _____  ___    __  ___________  _______
                     /"     "||" \  (\"   \|"  \  |" \("     _   ")/" __   )
                    (: ______)||  | |.\\   \    | ||  |)__/  \\__/(__/ _) ./
                     \/    |  |:  | |: \.   \\  | |:  |   \\_ /       /  //
                     // ___)  |.  | |.  \    \. | |.  |   |.  |    __ \_ \\
                    (:  (     /\  |\|    \    \ | /\  |\  \:  |   (: \__) :\
                     \__/    (__\_|_)\___|\____\)(__\_|_)  \__|    \_______)
```

Fast init for Linux
===================
[![Travis Status]][Travis] [![Coverity Status]][Coverity Scan]

Table of Contents
-----------------

* [Introduction](#introduction)
* [Features](#features)
* [/etc/finit.conf](doc/config.md#etcfinitconf)
* [/etc/finit.d](doc/config.md#etcfinitd)
* [Runparts & /etc/rc.local](#runparts--etcrclocal)
* [Bootstrap](#bootstrap)
* [Runlevels](#runlevels)
* [Inetd](doc/inetd.md#inetd)
* [Hooks, Callbacks & Plugins](doc/plugins.md#hooks-callbacks--plugins)
* [Rebooting & Halting](#rebooting--halting)
* [Commands & Status](#commands--status)
* [Building](#building)
* [Running](#running)
* [Debugging](#debugging)
* [Origin & References](#origin--references)


Introduction
------------

Finit is a plugin-based init with [process supervision][1] similar to
that of D.J. Bernstein's [daemontools][2] and Gerrit Pape's [runit][3].
The main focus of Finit is on small and embedded GNU/Linux systems, yet
fully functional on standard server and desktop installations as well.

Traditional [SysV init][4] style systems are scripted.  For low-resource
embedded systems this is quite resource intensive and often leads to
long boot times.  Finit reduces context switches and forking of shell
scripts to provide a system bootstrap written entirely in C.

There is no `/etc/init.d/rcS` script, or similar.  Instead configuration
is read from the main [/etc/finit.conf](doc/config.md#etcfinitconf),
which details kernel modules to load and bootstrap services to start.
After initial bootstrap, including setting up networking,
[/etc/finit.d/](doc/config.md#etcfinitd) and the familiar
[/etc/rc.local](#runparts--etcrclocal) are run.

**Example /etc/finit.conf:**

```
    # Fallback if /etc/hostname is missing
    host myhostname
    
    # Devices to fsck at boot
    check /dev/vda1
    
    # Runlevel to start after bootstrap, runlevel 'S'
    runlevel 2
    
    # Network bringup, not needed if /etc/network/interfaces exist
    #network service networking start
    #network /sbin/ifup -a

    # Services to be monitored and respawned as needed
    service [S12345] /sbin/watchdogd -L -f                       -- System watchdog daemon
    service [S12345] /sbin/syslogd -n -b 3 -D                    -- System log daemon
    service [S12345] /sbin/klogd -n                              -- Kernel log daemon
    service    2345] /sbin/lldpd -d -c -M1 -H0 -i                -- LLDP daemon (IEEE 802.1ab)
    
    # For multiple instances of the same service, add :ID somewhere between
    # the service/run/task keyword and the command.
    service :1 [2345] /sbin/httpd -f -h /http -p 80   -- Web server
    service :2 [2345] /sbin/httpd -f -h /http -p 8080 -- Old web server

    # Alternative method instead of below runparts, can also use /etc/rc.local
    #task [S] /etc/init.d/keyboard-setup start -- Setting up preliminary keymap
    #task [S] /etc/init.d/acpid start          -- Starting ACPI Daemon
    #task [S] /etc/init.d/kbd start            -- Preparing console
    #run [2] /etc/init.d/networking start      -- Start networking

    # Inetd services to start on demand, with alternate ports and filtering
    inetd ftp/tcp          nowait [2345] /sbin/uftpd -i -f       -- FTP daemon
    inetd tftp/udp           wait [2345] /sbin/uftpd -i -y       -- TFTP daemon
    inetd time/udp           wait [2345] internal                -- UNIX rdate service
    inetd time/tcp         nowait [2345] internal                -- UNIX rdate service
    inetd 3737/tcp         nowait [2345] internal.time           -- UNIX rdate service
    inetd telnet/tcp       nowait [2345] /sbin/telnetd -i -F     -- Telnet daemon
    inetd 2323/tcp         nowait [2345] /sbin/telnetd -i -F     -- Telnet daemon
    inetd 222/tcp@eth0     nowait [2345] /sbin/dropbear -i -R -F -- SSH service
    inetd ssh/tcp@*,!eth0  nowait [2345] /sbin/dropbear -i -R -F -- SSH service
    
    # Run start scripts from this directory
    # runparts /etc/start.d
    
    # Virtual consoles to start getty on
    tty [12345] /dev/tty1    115200 linux
    tty [12345] /dev/ttyAMA0 115200 vt100
```

For an example of a full blown embedded Linux, see [TroglOS][9] or the
`contrib/` section with [Alpine Linux support](contrib/alpine/)


Features
--------

**Process Supervision**

Start, monitor and restart services should they fail.


**Inetd**

Finit comes with a built-in [inetd server](doc/inetd.md).  No need to
maintain a separate config file for services that you want to start on
demand.

All inetd services started can be filtered per port and inbound
interface, reducing the need for a full blown firewall.

Built-in optional inetd services:

- echo RFC862
- chargen RFC864
- daytime RFC867
- discard RFC863
- time (rdate) RFC868

For more information, see [doc/inetd.md](doc/inetd.md).

**Runlevels**

Runlevels are optional in Finit, but support for [SysV runlevels][5] is
available if needed.  All services in runlevel S(1) are started first,
followed by the desired run-time runlevel.  Runlevel S can be started in
sequence by using `run [S] cmd`.  Changing runlevels at runtime is done
like any other init, e.g. <kbd>init 4</kbd>


**Plugins**

Finit plugins can be *callbacks*, *boot hooks* into different stages of
the boot process, or *pure extensions*.  Plugins are written in C and
compiled into a dynamic library that is loaded automatically by finit at
boot.  A basic set of plugins that extend and modify the basic behavior
are bundled in the `plugins/` directory.

Capabilities:

- **Task/Run/Service callbacks**  
  Modify service arguments, override service monitor's decisions to
  start/stop/reload a service.
- **Hooks**  
  Hook into the boot at predefined points to extend Finit
- **I/O**  
  Listen to external events and control Finit behavior/services
- **Inetd**  
  Extend Finit with internal inetd services, for an example, see
  `plugins/time.c`

Extensions and functionality not purely related to what an `/sbin/init`
needs to start a system are available as a set of plugins that either
hook into the boot process or respond to various I/O.

For more information, see [doc/plugins.md](doc/plugins.md).


Runparts & /etc/rc.local
------------------------

At the end of the boot, when networking and all services are up, finit
calls its built-in [run-parts(8)][] on the `runparts <DIR>` directory,
and `/etc/rc.local`, in that order if they exist.

```shell
    runparts /etc/rc.d/
```

No configuration stanza in `/etc/finit.conf` is required for `rc.local`.
If it exists and is an executable shell script, finit calls it at the
very end of the boot, before calling the `HOOK_SYSTEM_UP`.  See more on
hooks in [doc/plugins.md](doc/plugins.md#hooks), and the system bootstrap
below.


Bootstrap
---------

1. Populate `/dev`
2. Parse `/etc/finit.conf`
3. Load all `.so` plugins
4. Remount/Pivot `/` to get R+W
5. Call 1st level hooks, `HOOK_ROOTFS_UP`
6. Mount `/etc/fstab` and swap, if available
7. Cleanup stale files from `/tmp/*` et al
8. Enable SysV init signals
9. Call 2nd level hooks, `HOOK_BASEFS_UP`
10. Start all 'S' runlevel tasks and services
11. Load kernel parameters from `/etc/sysctl.conf`
12. Set hostname and bring up loopback interface
13. Call `network` script, if set in `/etc/finit.conf`
14. Call 3rd level hooks, `HOOK_NETWORK_UP`
15. Load all `*.conf` files in `/etc/finit.d/` and switch to the active
    active runlevel, as set in `/etc/finit.conf`, default is 2.  Here is
    where the rest of all tasks and inetd services are started.
16. Call 4th level hooks, `HOOK_SVC_UP`
17. If `runparts <DIR>` is set, [run-parts(8)][] is called on `<DIR>`
18. Call `/etc/rc.local`, if it exists and is an executable shell script
19. Call 5th level (last) hooks, `HOOK_SYSTEM_UP`
20. Start TTYs defined in `/etc/finit.conf`, or rescue on `/dev/console`

In (10) and (15) tasks and services defined in `/etc/finit.conf` are
started.  Remember, all `service` and `task` stanzas are started in
parallel and `run` in sequence, and in the order listed.  Hence, to
emulate a SysV `/etc/init.d/rcS` one could write a long file with only
`run` statements.

Notice the five hook points that are called at various point in the
bootstrap process.  This is where plugins can extend the boot in any way
they please.

For instance, at `HOOK_BASEFS_UP` a plugin could read an XML file from a
USB stick, convert/copy its contents to the system's `/etc/` directory,
well before all 'S' runlevel tasks are started.  This could be used with
system images that are created read-only and all configuration is stored
on external media.


Runlevels
---------

Basic support for [runlevels][5] is included in Finit from v1.8.  By
default all services, tasks, run commands and TTYs listed without a set
of runlevels get a default set `[234]` assigned.  The default runlevel
after boot is 2.

Finit supports runlevels 0-9, and S, with 0 reserved for halt, 6 reboot
and S for services to only run at bootstrap.  Runlevel 1 is the single
user level, where usually no networking is enabled.  In Finit this is
more of a policy for the user to define.  Normally only runlevels 1-6
are used, and even more commonly, only the default runlevel is used.

To specify an allowed set of runlevels for a `service`, `run` command,
`task`, or `tty`, add `[NNN]` to your `/etc/finit.conf`, like this:

```shell
    service [S12345] /sbin/syslogd -n -x     -- System log daemon
    run     [S]      /etc/init.d/acpid start -- Starting ACPI Daemon
    task    [S]      /etc/init.d/kbd start   -- Preparing console
    service [S12345] /sbin/klogd -n -x       -- Kernel log daemon
    tty     [12345]  /dev/tty1
    tty     [2]      /dev/tty2
    tty     [2]      /dev/tty3
    tty     [2]      /dev/tty4
    tty     [2]      /dev/tty5
    tty     [2]      /dev/tty6
```

In this example syslogd is first started, in parallel, and then acpid is
called using a conventional SysV init script.  It is called with the run
command, meaning the following task command to start the kbd script is
not called until the acpid init script has fully completed.  Then the
keyboard setup script is called in parallel with klogd as a monitored
service.

Again, tasks and services are started in parallel, while run commands
are called in the order listed and subsequent commands are not started
until a run command has completed.

Switching between runlevels can be done by calling init with a single
argument, e.g. <kbd>init 5</kbd> switches to runlevel 5.  When changing
runlevels Finit also automatically reloads all `.conf` files in the
`/etc/finit.d/` directory.  So if you want to set a new system config,
switch to runlevel 1, change all config files in the system, and touch
all `.conf` files in `/etc/finit.d` before switching back to the
previous runlevel again — that way Finit can both stop old services and
start any new ones for you, without rebooting the system.


Rebooting & Halting
-------------------

Finit handles `SIGUSR1` and `SIGUSR2` for reboot and halt, and listens
to `/dev/initctl` so system reboot and halt commands also work.  The
latter is implemented in the optional `initctl.so` plugin and can be
accessed with the traditional `telinit` command line tool, symlinked to
`finit`.  Hence, if finit is your system init, then `init q` will work
as the UNIX beards intended.

```shell
    ~ $ telinit
    Usage: telinit [OPTIONS] [q | Q | 0-9]
    
    Options:
      -h, --help            This help text
      -v, --version         Show Finit version
    
    Commands:
      q | Q           Reload *.conf in /etc/finit.d/, like SIGHUP
      0 - 9           Change runlevel: 0 halt, 6 reboot
```


Commands & Status
-----------------

Finit also implements a more modern API to query status, and start/stop
services, called `initctl`.  Unlike `telinit` the `initctl` tool does
not return until the given command has fully completed.

```shell
    ~ $ initctl -h
    Usage: initctl [OPTIONS] <COMMAND>
    
    Options:
      -d, --debug               Debug initctl (client)
      -v, --verbose             Verbose output
      -h, --help                This help text
    
    Commands:
      debug                     Toggle Finit (daemon) debug
      help                      This help text
      reload                    Reload *.conf in /etc/finit.d/ and activate changes
      runlevel [0-9]            Show or set runlevel: 0 halt, 6 reboot
      status | show             Show status of services
      cond     set   <COND>     Set (assert) condition     => +COND
      cond     clear <COND>     Clear (deassert) condition => -COND
      cond     flux  <COND>     Emulate flux condition     => ~COND
      cond     show             Show condition status
      start    <JOB|NAME>[:ID]  Start service by job# or name, with optional ID
      stop     <JOB|NAME>[:ID]  Stop/Pause a running service by job# or name
      restart  <JOB|NAME>[:ID]  Restart (stop/start) service by job# or name
      reload   <JOB|NAME>[:ID]  Reload (SIGHUP) service by job# or name
      version                   Show Finit version
```

The `<!>` notation to a service stanza can be used empty, then it will
apply to `reload` and `runlevel` commands.  I.e., when a service's
`.conf` file has been changed Finit will stop and start it instead.  If
a service does *not* have `<!>` declared, the service will be reloaded
(`SIGHUP`:ed) in the `START` phase instead.  The latter is the preferred
behaviour, but not all daemons support this, unfortunately.

**Note:** even though it is possible to start services not belonging to
the current runlevel these services will not be respawned automatically
by Finit if they exit (crash).  Hence, if the runlevel is 2, the below
Dropbear SSH service will not be restarted if it is killed or exits.

```shell
    ~ $ initctl status -v
    1       running  476     [S12345]   /sbin/watchdog -T 16 -t 2 -F /dev/watchdog
    2       running  477     [S12345]   /sbin/syslogd -n -b 3 -D
    3       running  478     [S12345]   /sbin/klogd -n
    4:1       inetd  0       [2345]     internal time allow *:37
    4:2       inetd  0       [2345]     internal time allow *:37
    4:3       inetd  0       [2345]     internal 3737 allow *:3737
    5:1       inetd  0       [2345]     /sbin/telnetd allow *:23 deny eth0,eth1
    5:2       inetd  0       [2345]     /sbin/telnetd allow eth0:2323,eth2:2323,eth1:2323
    6:1       inetd  0       [345]      /sbin/dropbear allow eth0:222
    6:2       inetd  0       [345]      /sbin/dropbear allow *:22 deny eth0
```


Building
--------

Finit comes with a traditional configure script to control features and
optional plugins to enable.  It does however depend on two external
libraries that provide some frog DNA needed:

- [libuEv][]
- [libite][] (-lite)

Like most free/open source software that uses `configure` they deafult
to install to `/usr/local`.  However, some Linux distributions do no
longer search that path for installed software, e.g. Fedora and Alpine
Linux.  To get finit's configure script to find its dependencies you
have to help the `pkg-config` tool a bit if you do not change the
default prefix path:

    PKG_CONFIG_LIBDIR=/usr/local/lib/pkgconfig ./configure

Below are a few of the main switches to
configure:

* `--disable-inetd`: Disable the built-in inetd server.

* `--enable-embedded`: Target finit for BusyBox getty and mdev instead
  of a standard Linux distribution with GNU tools and udev.

* `--enable-rw-rootfs`: Most desktop and server systems boot with the
  root file stystem read-only.  With this setting Finit will remount it
  as read-write early at boot so the `bootmisc.so` plugin can run.
  Usually not needed on embedded systems.

* `--enable-static`: Build Finit statically.  The plugins will be
  built-ins (.o files) and all external libraries, except the C library
  will be linked statically.

* `--enable-alsa-utils`: Enable the optional `alsa-utils.so` sound plugin.

* `--enable-dbus`: Enable the optional D-Bus `dbus.so` plugin.

* `--enable-lost`: Enable noisy example plugin for `HOOK_SVC_LOST`.

* `--enable-resolvconf`: Enable the `resolvconf.so` optional plugin.

* `--enable-x11-common`: Enable the optional X Window `x11-common.so` plugin.

For more configure flags, see <kbd>./configure --help</kbd>

**Example**

First, unpack the archive:

```shell
    $ tar xf finit-3.0.tar.xz
    $ cd finit-3.0/
```

Then configure, build and install:

```shell
    $ ./configure --enable-embedded --enable-rw-rootfs --enable-inetd-echo-plugin \
                  --enable-inetd-chargen-plugin --enable-inetd-daytime-plugin     \
                  --enable-inetd-discard-plugin --enable-inetd-time-plugin        \
                  --with-heading="Alpine Linux 3.4" --with-hostname=alpine
    $ make
    .
    .
    .
    $ DESTDIR=/tmp/finit make install
```

In this example the [finit-3.0.tar.xz][10] archive is unpacked to the
user's home directory, configured, built and installed to a temporary
staging directory.  The environment variable `DESTDIR` controls the
destination directory when installing, very useful for building binary
standalone packages.

To target an embedded Linux system, usally a system that use BusyBox
tools instead of udev & C:o, add <kbd>--enable-embedded</kbd> to the
configure command above.  This enables `mdev` instead of `udev` and the
BusyBox `getty` syntax.  Remember to also change the Linux config to:

    CONFIG_UEVENT_HELPER_PATH="/sbin/mdev"

**Note:** If you run into problems starting Finit, take a look at
  `finit.c`.  One of the most common problems is a custom Linux kernel
  build that lack `CONFIG_DEVTMPFS`.  Another is too much cruft in the
  system `/etc/fstab`.


Running
-------

The default install does not setup finit as the system default
`/sbin/init`, neither does it setup an initial `/etc/finit.conf`.

It is assumed that users of finit are competent enough to either setup
finit as their default `/sbin/init` or alter their respective GRUB,
LOADLIN, LILO, U-Boot/Barebox or RedBoot boot loader configuration to
give the kernel the following extra command line:

```shell
    init=/sbin/finit
```

![Finit starting Debian 6.0](images/finit-screenshot.jpg "Finit screenshot")


Debugging
---------

Add `finit_debug`, or `--debug`, to the kernel command line to enable
debug messages.  See the output from `configure --help` for more on
how to assist debugging.

```shell
    init=/sbin/finit --debug
```


Origin & References
-------------------

This project is based on the [original finit][] by [Claudio Matsuoka][]
which was reverse engineered from syscalls of the [EeePC fastinit][] —
"gaps filled with frog DNA …"

Finit is developed and maintained by [Joachim Nilsson][] at [GitHub][].
Please file bug reports, clone it, or send pull requests for bug fixes
and proposed extensions.


[1]:  https://en.wikipedia.org/wiki/Process_supervision
[2]:  http://cr.yp.to/daemontools.html
[3]:  http://smarden.org/runit/
[4]:  http://en.wikipedia.org/wiki/Init
[5]:  http://en.wikipedia.org/wiki/Runlevel
[9]:  https://github.com/troglobit/troglos
[10]: ftp://troglobit.com/finit/finit-3.0.tar.xz
[run-parts(8)]:     http://manpages.debian.org/cgi-bin/man.cgi?query=run-parts
[original finit]:   http://helllabs.org/finit/
[EeePC fastinit]:   http://wiki.eeeuser.com/boot_process:the_boot_process
[Claudio Matsuoka]: https://github.com/cmatsuoka
[Joachim Nilsson]:  http://troglobit.com
[GitHub]:           https://github.com/troglobit/finit
[libuEv]:           https://github.com/troglobit/libuev
[libite]:           https://github.com/troglobit/libite
[Travis]:           https://travis-ci.org/troglobit/finit
[Travis Status]:    https://travis-ci.org/troglobit/finit.png?branch=master
[Coverity Scan]:    https://scan.coverity.com/projects/3545
[Coverity Status]:  https://scan.coverity.com/projects/3545/badge.svg

<!--
  -- Local Variables:
  -- mode: markdown
  -- End:
  -->
