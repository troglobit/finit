TODO
====

Unsorted TODO list of features and points of investigation that could be
useful for improving finit.  But what is an improvement to finit?  Well,
similiar to launchd, the goal of finit is to be a FAST replacement for:

  * init
  * inetd
  * crond
  * watchdogd

The main goal is to be FAST. Secondary goals are to be: small, simple
and with a very low dependency on external packages.


Inetd
-----

Simple way of running Internet services on demand, like the old inetd:

    inetd SVC [RUNLVL] /path/to/daemon args -- description

Example:

    # Inetd services, launched on demand
    inetd ssh [2345] /usr/sbin/sshd -i -- SSH Daemon

In keeping with the finit tradition, an optional callback can be setup
to each inetd service.  When a client connects the finit will call the
callback with a `struct in_pktinfo` argument which the callback then can
allowed or deny.

Alternatively, borrowing from TCP wrappers by Wietse Wenema, a set of
`allow` and `deny` directives can be added to `finit.conf` to control
inetd services.  It could be what IP addreses, networks or interfaces
are allowed to connect to the given inetd services.  To start with the
easiest (and what we need for WeOS at Westermo) is allow/deny per iface.

    # Disable access to SSH
    deny  ssh all
    # Allow SSH connections originating from eth0
    allow ssh eth0


Priority
--------

The intent of finit is to monitor not just processes, but to supervise
an entire system and the behavior of certain processes that declare
themselves to be finit compliant.

  * Integrate libev, or libuev, to handle events: signals, I/O, etc.

    "Event driven software improves concurrency" -- Dave Zarzycki, Apple

    See the [launchd video](http://www.youtube.com/watch?v=cD_s6Fjdri8)
    for more info!
  * Add support for a `/dev/watchdog` plugin to replace `watchdogd`
  * Integrate watchdog plugin with process/system supervisor to optionally
    reboot the system when a process stops responding, or when a respawn
    limit has been reached, as well as when system load gets too high.


Configuration
-------------

  * Add support for `init <q | reload-configuration>` and `SIGHUP` to
    have finit reload its `finit.conf`, but also
  * Add support for inotify to automatically reload `finit.conf`


Runlevels
---------

  * Implement `initctl stop|start|restart|reload|status <SVC>` and
    `service <SVC> stop|start|restart|reload|status` on top
  * Add PRE and POST hooks for when switching between runlevels


Miscellaneous
-------------

  * Add support for `init -v,--version | version`
  * Cleanup move sources from top-level directory to `src/` and `include/`
  * Make sure to install `queue.h` to `$(PREFIX)/include/finit/queue.h`
  * Move `finit.conf` "check" command to plugin which checks `/etc/fstab`
    instead.  This is the de-facto practice.  But keep the check command
    for really low-end systems w/o `/etc/fstab`.


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

