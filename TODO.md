Goals of Finit
==============

The intent of finit is to monitor not just processes, but to supervise
an entire system and the behavior of certain processes that declare
themselves to be finit compliant.  Similar to launchd, the goal of
finit is to be a replacement for:

* init
* inetd
* crond
* watchdogd

A small and simple replacement, primarily for embedded systems, with a
very low dependency on external packages.


General
-------

* Add compile-time supoprt for running `/bin/sh` instead of `getty`
* Add support for `init <q | reload-configuration>` and `SIGHUP` to
  have finit reload its `finit.conf`, but also
* Add support for inotify to automatically reload `finit.conf`
* Implement `initctl stop|start|restart|reload|status <SVC>` and
  `service <SVC> stop|start|restart|reload|status` on top
* Add PRE and POST hooks for when switching between runlevels
* Add support for `init -v,--version | version`
* Cleanup move sources from top-level directory to `src/` and `include/`
* Make sure to install `queue.h` to `$(PREFIX)/include/finit/queue.h`
* Move `finit.conf` "check" command to plugin which checks `/etc/fstab`
  instead.  This is the de-facto practice.  But keep the check command
  for really low-end systems w/o `/etc/fstab`.


Init
----

Finit in itself is not SysV Init compatible, it doesn't use `/etc/rc.d`
and `/etc/inet.d` scripts to boot the system.  However, a plausible way
to achieve compatibility would be to add a plugin to finit which reads
`/etc/inittab` to be able to start standard Linux systems.


Inetd
-----

Simple way of running Internet services on demand, like the old inetd:

    inetd PORT/PROTO <wait|nowait> [@USER[:GROUP]] /path/to/daemon args -- desc

Examples:

    # Inetd services, launched on demand
    inetd ssh/tcp nowait [2345] @root:root /usr/sbin/sshd -i -- SSH daemon

In keeping with the finit tradition, an optional callback can be setup
to each inetd service.  When a client connects the finit will call the
callback with a `struct in_pktinfo` argument which the callback then can
allowed or deny.

Alternatively, borrowing slightly from TCP wrappers by Wietse Venema, a
set of `allow` and `deny` directives can be added to `finit.conf` to
control inetd services.  It could be what IP addreses, networks or
interfaces are allowed to connect to the given inetd services.  To start
with the easiest (and what we need for WeOS at Westermo) is allow/deny
per iface.

    # Disable access to SSH
    deny  ssh all
    # Allow SSH connections originating from eth0
    allow ssh eth0

Another option, to further extend the Inetd capabilities, is a separate
`finetd.conf` to list services.  This would be beneficial when running
an SSH service on different ports on different interfaces:

    # Run SSH on port 22 on all interfaces except upstream
    tcp ssh {
       listen = *, !wan0
       exec   = dropbear -i
    }
    
    # Run SSH on non-standard port 222 for upstream interface
    tcp ssh {
       port   = 222
       listen = wan0
       exec   = dropbear -i
       wait   = yes
    }
    
    # Run TFTP service on all interfaces except upstream interface
    udp tftp {
       listen = *, !wan0
       exec   = uftpd -i -t
       user   = root
    }

or, more concise

    # System has four interfaces: wan0, eth0, eth1, eth2
    # This runs SSH on only three, with various ports
    tcp ssh {
       listen = wan0:222 eth0:22 eth1:22
       exec   = /usr/sbin/dropbear -i
       wait   = yes
    }

this would be equivalent

    tcp ssh {
       listen = wan0:222 !eth2
       exec   = /usr/sbin/dropbear -i
       wait   = yes
    }

Syntax similar to [xinetd](http://en.wikipedia.org/wiki/Xinetd)

### Custom Inet Port for Common Services

- How to do this, without an obtrusive syntax?

Maybe like this?  Then we can still refer to it as the time protocol,
and have multiple entries.

    inetd time:8037/udp wait [2345] @root:root internal
    inetd time:3737/udp wait [2345] @root:root internal

But what if we want to have different allow/deny rules per service?
Here's a proposal, similar to user/pass encoding in URI's.  This variant
could potentially be a lot easier to implement and also to maintain for
users, compared to the above allow/deny/xinetd variants.

    inetd ssh@eth0:222/tcp nowait [2345] /usr/sbin/sshd -i
    inetd ssh@eth1:22/tcp  nowait [2345] /usr/sbin/sshd -i

or

    inetd ssh@eth0:222/tcp nowait [2345] /usr/sbin/sshd -i
    inetd ssh@*:22/tcp     nowait [2345] /usr/sbin/sshd -i

or

    inetd ssh@eth0:222/tcp nowait [2345] /usr/sbin/sshd -i
    inetd ssh/tcp          nowait [2345] /usr/sbin/sshd -i

All three versions would be equivalent, except for the first one when
there are more interfaces than just eth0 and eth1.  The latter two would
however be equivalent.

Multiple `inetd ssh` entries with the same port will not be unique, they
will reuse all settings of the first listed entry.  However, an entry
with a unique port and interface will be registered as a unique service.

**Note:** Also, there is currently no verification that the same port is
  used more than once.  So a standard http service will clash with an
  ssh entry ssh@*:80/tcp ... why you would ever want that?

Inetd callbacks are no different from other callbacks.  They match to an
inetd line using the command.  So multiple `/usr/bin/sshd` inetd lines
in `finit.conf` will all use the same callback.  However, the callback
will be called with the Internet port and interface as arguments.  Like
this:

    svc_cmd_t sshd_callback(svc_t *svc, int port, char *ifname)
    {
        if (!cfg_is_ssh_enabled(ifname, port))
            return SVC_STOP;
    
        return SVC_START;
    }

As usual, the callback is run in a separate process context and it is up
to the implementer to return `SVC_STOP` or `SVC_START` (`SVC_RELOAD` is
not applicable for inetd services).


### Internal inetd services

There are a few standard services that inetd usually had built-in:

- time
- echo
- chargen
- discard

At least 'time' will be implemented in finit, as a plugin, to serve as a
simple means of testing inetd functionality stand-alone, but also as a
very rudimentary time server for rdate clients.

Example:

    # Built-in inetd time service, launched on demand
    inetd time/udp wait [2345] @root:root internal


Crond
-----

Requirements are quite rudimentary, basic cron functionality, mainly
system-level timed periodic tasks:

* Periodically run a task, in a matching runlevel
* Optionally run as non-root (`crontab -e` as operator)

Proposed syntax:

    # Crontab
    cron @YY-mm-ddTHH:MM [LVLS] /path/to/cmd [ARGS] -- Optional descr
    
    # One-shot 'at' command
    at @YY-mm-ddTHH:MM [LVLS] /path/to/cmd [ARGS] -- Optional descr

Notice the ISO date in notation.  The runlevels is extra filtering
sugar.  E.g., if cron service is only allowed in runlevels two or three
it will not start if system currently is in runlevel four.

To run a command as another user the `.conf` file must have a that
owner.  E.g., `/etc/finit.d/extra.conf` may be owned by operator and
only hold `cron ...` lines.

What would be extremely useful is if `initctl`, or a homegrown `at`,
could support the basic functionality of the `at` command directly from
the shell -- that way setting up one-time jobs would not entail
re-reading `/etc/finit.conf`


Watchdog
--------

Support for monitoring the health of the system and its processes.

* Add support for a `/dev/watchdog` built-in to replace both the BusyBox
  watchdogd and my own refreshed uClinux-dist adaptation,
  [watchdogd](https://github.com/troglobit/watchdogd)
* Add a system supervisor (pmon) to optionally reboot the system when a
  process stops responding, or when a respawn limit has been reached, as
  well as when system load gets too high.  For details on UNIX loadavg,
  see http://stackoverflow.com/questions/11987495/linux-proc-loadavg
  - Default to 0.7 as MAX recommended load before warning and 0.9 reboot
* API for processes to register with the watchdog, libwdt
  - Deeper monitoring of a process' main loop by instrumenting
  - If a process is not heard from within its subscribed period time
    reboot system or restart process.
  - Add client libwdt library, inspired by old Westermo API and the
    [libwdt] API published in the [Fritz!Box source dump], take for
    instance the `fritzbox7170-source-files-04.87-tar.gz` drop and see
    the `GPL-release_kernel.tgz` in `drivers/char/avm_new/` for the
    kernel driver, and the `LGPL-GPL-release_target_tools.tgz` in `wdt/`
    for the userland API.
  - When a client process (a standard app/daemon instrumented with
    libwdt calls in its main loop) registers with the watchdog, we raise
    the RT priority to 98 (just below the kernel watchdog in prio).
    This to ensure that system monitoring goes before anything else in
    the system.
  - Use UNIX domain sockets for communication between daemon and clients
* Separate `/etc/watchdog.conf` configuration file, or a perhaps
  support for a more generic `/etc/finit.d/PLUGIN.conf`?
* Support enable/disable watchdog features:
  - Supervise processes,
  - CPU loadavg,
  - Watch for file descriptor leaks,
  - etc.


Documentation
-------------

* Write man pages for finit and `finit.conf`, steal from the excellent
  `pimd` man pages ...
* Update Debian `finit.conf` example with runlevels, `tty/console` and
  dependency handling for Debian 8 (the dreaded systemd release).


Investigation
-------------

* [FHS changes](http://askubuntu.com/questions/57297/why-has-var-run-been-migrated-to-run)
  affecting runtime status, plugins, etc.

[libwdt]:                http://www.wehavemorefun.de/fritzbox/Libwdt.so
[Fritz!Box source dump]: ftp://ftp.avm.de/fritz.box/fritzbox.fon_wlan_7170/x_misc/opensrc/

<!--
  -- Local Variables:
  -- mode: markdown
  -- End:
  -->
