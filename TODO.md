TODO
====

The intent of finit is to monitor not just processes, but to supervise
an entire system and the behavior of certain processes that declare
themselves to be finit compliant.  Similiar to launchd, the goal of
finit is to be a replacement for:

  * init
  * inetd
  * crond
  * watchdogd

A small and simple replacement, primarily for embedded systems, with a
very low dependency on external packages.

  * Integrate libev, or libuEv, to handle events: signals, I/O, etc.

    "Event driven software improves concurrency" -- Dave Zarzycki, Apple

    See the [launchd video](http://www.youtube.com/watch?v=cD_s6Fjdri8)
    for more info!

    See uftpd for how libuEv can be easily integrated as a submodule.
  * Add support for a `/dev/watchdog` plugin to replace `watchdogd`
  * Integrate watchdog plugin with process/system supervisor to optionally
    reboot the system when a process stops responding, or when a respawn
    limit has been reached, as well as when system load gets too high.


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

Another option, to further extend the Inetd capabilities, is a separate
`finetd.conf` to list services.  This would be beneficial when running
an SSH service on different ports on different interfaces:

    # Run SSH on port 22 on all interfaces except upstream
    tcp ssh {
       listen = *, !wan0
       exec   = dropbear -i
    }
    
    # Run SSH on non-standard port 222 for upstream interface
    tcp 222 {
       listen = wan0
       exec   = dropbear -i
    }
    
    # Run TFTP service on all interfaces except upstream interface
    udp tftp {
       listen = *, !wan0
       exec   = uftpd -i -t
    }


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

