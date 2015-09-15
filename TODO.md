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

* Add compile-time support for running `/bin/sh` instead of `getty`
* Add `!` or `[wait <pidfile>]` to service directive to let finit pause
  and wait for a pidfile before starting processes -- both at boot and
  when starting processes in a runlevel.
* Allow custom reload for processes like libreSwan's pluto

        service [2345] <!gw,if:eth0,reload:'starter reload'> /sbin/pluto

* Allow process callbacks to be scripts.
* Add support for a pipe/popen where we can listen for `STOP`, `START`
  and `RELOAD` on `STDOUT` from a script ... call it for example
  `.monitor=` in `svc_t` ... the script named as the basename of the
  service it monitors + `.sh`.
* Add support for `/etc/sysctl.d`
* Implement `initctl stop|start|restart|reload|status <SVC>` and
  `service <SVC> stop|start|restart|reload|status` on top
* Add PRE and POST hooks for when switching between runlevels
* Make sure to install `queue.h` to `$(PREFIX)/include/finit/queue.h`
* Move `finit.conf` "check" command to plugin which checks `/etc/fstab`
  instead.  This is the de-facto practice.  But keep the check command
  for really low-end systems w/o `/etc/fstab`.
* Add support for generic events (strings or ubus events) that are passed
  verbatim (?) to the service stanza definitions.


Init
----

Finit in itself is not SysV Init compatible, it doesn't use `/etc/rc.d`
and `/etc/inet.d` scripts to boot the system.  However, a plausible way
to achieve compatibility would be to add a plugin to finit which reads
`/etc/inittab` to be able to start standard Linux systems.


Inetd
-----

Optimize interface filtering by using socket filter.  The functions
`inet_*_peek()` and `inetd_is_allowed()` used for interace filtering
should be possible to rewrite as socket filters.


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
