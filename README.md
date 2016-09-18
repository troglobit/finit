```
                 _______  __    _____  ___    __  ___________  _______
                /"     "||" \  (\"   \|"  \  |" \("     _   ")/" __   )
               (: ______)||  | |.\\   \    | ||  |)__/  \\__/(__/ _) ./
                \/    |  |:  | |: \.   \\  | |:  |   \\_ /       /  //
                // ___)  |.  | |.  \    \. | |.  |   |.  |    __ \_ \\
               (:  (     /\  |\|    \    \ | /\  |\  \:  |   (: \__) :\
                \__/    (__\_|_)\___|\____\)(__\_|_)  \__|    \_______) ... Yes, we need a new logo!
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
* [Bootstrap](doc/bootstrap.md#bootstrap)
* [Runlevels](#runlevels)
* [Inetd](doc/inetd.md#inetd)
* [Hooks, Callbacks & Plugins](doc/plugins.md#hooks-callbacks--plugins)
* [Rebooting & Halting](#rebooting--halting)
* [Commands & Status](#commands--status)
* [Building](doc/build.md#building)
* [Running](#running)
* [Debugging](#debugging)
* [Origin & References](#origin--references)


Introduction
------------

Finit is an EeePC inspired Fastinit clone with [process supervision][1]
similar to that of D.J. Bernstein's [daemontools][2] and Gerrit Pape's
[runit][3].  The focus of Finit is on small and embedded Linux systems,
although fully usable on server and desktop installations as well.  See
the [contrib section](contrib/) for Debian and Alpine Linux examples.

Traditional [SysV init][4] style systems are scripted.  For low-resource
embedded systems this is quite resource intensive and often leads to
long boot times.  Finit reduces context switches and forking of shell
scripts to provide a swift [system bootstrap](doc/bootstrap.md) written
entirely in C.

Configuration is read from [/etc/finit.conf](doc/config.md#etcfinitconf)
which details kernel modules to load, services and TTYs to start.  When
initial [bootstrap](doc/bootstrap.md) is done, including setting up
networking, [/etc/finit.d/](doc/config.md#etcfinitd) and the familiar
[/etc/rc.local](#runparts--etcrclocal) are run.

**Example /etc/finit.conf:**

```
    # Fallback if /etc/hostname is missing
    host wopr
    
    # Runlevel to start after bootstrap, runlevel 'S'
    runlevel 2
    
    # Services to be monitored and respawned as needed
    service [S12345] /sbin/watchdogd -L -f                       -- System watchdog daemon
    service [S12345] /sbin/syslogd -n -b 3 -D                    -- System log daemon
    service [S12345] /sbin/klogd -n                              -- Kernel log daemon
    service   [2345] /sbin/lldpd -d -c -M1 -H0 -i                -- LLDP daemon (IEEE 802.1ab)
    
    # For multiple instances of the same service, add :ID somewhere between
    # the service/run/task keyword and the command.
    service :1 [2345] /sbin/merecat -n -p 80   /var/www -- Web server
    service :2 [2345] /sbin/merecat -n -p 8080 /var/www -- Old web server

    # Alternative method instead of below runparts, can also use /etc/rc.local
    #task [S] /etc/init.d/keyboard-setup start -- Setting up preliminary keymap
    #task [S] /etc/init.d/acpid start          -- Starting ACPI Daemon
    #task [S] /etc/init.d/kbd start            -- Preparing console

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
    
    # Virtual consoles to start built-in getty on
    tty [12345] /dev/tty1    115200 linux
    tty [12345] /dev/ttyAMA0 115200 vt100
```

For an example of a full blown embedded Linux, see [TroglOS][9], or take
a look at the `contrib/` section with [Alpine Linux](contrib/alpine/),
[Debian](contrib/debian/), support and more.


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

**Getty**

Finit also comes with a built-in Getty for Linux console TTYs.  It can
parse `/etc/inittab` and set the speed.  Then `/bin/login` handles all
nasty bits with PAM etc.

    # /etc/finit.conf
    tty [12345] /dev/tty1    38400  linux
    tty [12345] /dev/ttyAMA0 115200 vt100

**Runlevels**

Support for SysV init-style [runlevels][5] is available, in the same
minimal style as everything else in Finit.  The `[2345]` syntax can be
applied to service, task, run, inetd, and TTY stanzas.

All services in runlevel S(1) are started first, followed by the desired
run-time runlevel.  Runlevel S can be started in sequence by using `run
[S] cmd`.  Changing runlevels at runtime is done like any other init,
e.g. <kbd>init 4</kbd>, but also using the more advanced `intictl` tool.


**Plugins**

Plugins can be used to *extend* the functionality of Finit, *hook into*
different stages of the boot process and at runtime, and act as *pure
extensions*.  Plugins are written in C and compiled into a dynamic
library that is loaded automatically by finit at boot.  A basic set of
plugins are bundled in the `plugins/` directory.

Capabilities:

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

At the end of the boot, when networking and all services are up, Finit
calls its built-in [run-parts(8)][] on the `runparts <DIR>` directory,
and `/etc/rc.local`, in that order if they exist.

```shell
    runparts /etc/rc.d/
```

No configuration stanza in `/etc/finit.conf` is required for `rc.local`.
If it exists and is an executable shell script, finit calls it at the
very end of the boot, before calling the `HOOK_SYSTEM_UP`.  See more on
hooks in [doc/plugins.md](doc/plugins.md#hooks), and about the system
bootstrap in [doc/bootstrap.md](doc/bootstrap.md).


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

```
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

Traditionally, rebooting and halting a UNIX system is done by changing
its runlevel.  Finit comes with its own tooling providing: `shutdown`,
`reboot`, `poweroff`, and `suspend`, but also the traditional `init` and
`telinit`, as well as a more modern `initctl` tool, detailed in the next
section.

For compatibility reasons Finit listens to the same set of signals as
BusyBox init.  This is not 100% compatible with SysV init, but clearly
the more common combination for Finit.  For more details, see
[doc/signals.md](doc/signals.md).

Finit also listens to the classic SysV init FIFO, used by `telinit`.
Support for this is implemented by the `initctl.so` plugin.  Hence,
`telinit q` will work as the UNIX beards intended.

    ~ # telinit -h
    Usage: telinit [OPTIONS] [q | Q | 0-9]
    
    Options:
      -h, --help      This help text
      -V, --version   Show Finit version
    
    Commands:
      0               Power-off the system, same as initctl poweroff
      6               Reboot the system, same as initctl reboot
      2, 3, 4, 5      Change runlevel. Starts services in new runlevel, stops any
                      services in prev. runlevel that are not allowed in new.
      q, Q            Reload *.conf in /etc/finit.d/, same as initctl reload or
                      sending SIGHUP to PID 1
      1, s, S         Enter system rescue mode, runlevel 1


Commands & Status
-----------------

Finit also implements a more modern API to query status, and start/stop
services, called `initctl`.  Unlike `telinit` the `initctl` tool does
not return until the given command has fully completed.

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

For services *not* supporting `SIGHUP` the `<!>` notation in the .conf
file must be used to tell Finit to stop and start it on `reload` and
`runlevel` changes.  If `<>` holds more [conditions](doc/conditions.md),
these will also affect how a service is maintained.

**Note:** even though it is possible to start services not belonging in
the current runlevel these services will not be respawned automatically
by Finit if they exit (crash).  Hence, if the runlevel is 2, the below
Dropbear SSH service will not be restarted if it is killed or exits.

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


Running
-------

Having successfully [built Finit](doc/build.md) it may now be time to
take it for a test drive.  The `make install` attempts to set up finit
as the system system init, `/sbin/init`, but this is usually a symlink
pointing to the current init.

So either change the symlink, or change your boot loader (GRUB, LOADLIN,
LILO, U-Boot/Barebox or RedBoot) configuration to append the following
to the kernel command line:

```shell
    append="init=/sbin/finit"
```

Remember to also set up an initial `/etc/finit.conf` before rebooting!

![Finit starting TroglOS](images/finit3-screenshot.png "Finit screenshot")


Debugging
---------

Add `finit_debug`, or `--debug`, to the kernel command line to enable
debug messages.

```shell
    append="init=/sbin/finit --debug"
```

To debug startup issues, in particular issues with getty/login, try
`configure --enable-fallback-shell`.  When no TTYs are detected, and
Finit is configured with this option, Finit will try to start a bare
`/bin/sh` on the boot console.


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
[run-parts(8)]:     http://manpages.debian.org/cgi-bin/man.cgi?query=run-parts
[original finit]:   http://helllabs.org/finit/
[EeePC fastinit]:   http://wiki.eeeuser.com/boot_process:the_boot_process
[Claudio Matsuoka]: https://github.com/cmatsuoka
[Joachim Nilsson]:  http://troglobit.com
[GitHub]:           https://github.com/troglobit/finit
[Travis]:           https://travis-ci.org/troglobit/finit
[Travis Status]:    https://travis-ci.org/troglobit/finit.png?branch=master
[Coverity Scan]:    https://scan.coverity.com/projects/3545
[Coverity Status]:  https://scan.coverity.com/projects/3545/badge.svg

<!--
  -- Local Variables:
  -- mode: markdown
  -- End:
  -->
