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
low dependency on external packages.  A compliant software is one that
can run as a damon in the foreground.  If it can also touch its PID file
after a reconfiguration (`initctl reload`), the Finit dependency system
can also be used.


General
-------

* Remove shm, let `initctl` call `api.c` for each `svc_t` instead of
  opening up the inner parts of PID 1 to other processes.  Also, this
  would prevent issues like #81 from happening.
* Add support for timed shutdown in Finit, including cancelled shutdown.
* SysV init and systemd use SIGUSR1 to restart their FIFO/D-Bus.  Add
  API restart to SIGHUP callback for the same functionality in Finit.
* Add `finit.conf` support for UPS notification (SIGPWR) to start a task
  using, e.g. <sys/power/{ok,fail,low}> conditions.  More info in sig.c
* Add `finit.conf` support for ctrl-alt-delete (SIGINT) and kbrequest,
  i.e. KeyboardSignal, (SIGWINCH) behavior.  Using conditions to a task,
  e.g, <sys/key/ctrlaltdel> and <sys/key/signal> like SIGPWR handling.
* Add `IFF_RUNNING` support to netlink plugin along `IFF_UP`
* Allow scripts, instead of C code, as process callbacks
* Add support for a pipe/popen where we can listen for `STOP`, `START`
  and `RELOAD` on `STDOUT` from a script ... call it for example
  `.monitor=` in `svc_t` ... the script named as the basename of the
  service it monitors + `.sh`.
* Implement `initctl stop|start|restart|reload|status <SVC>` and
  `service <SVC> stop|start|restart|reload|status` on top
* Add support for JSON output, or similar, from `initctl show`, e.g.
  `initctl show --json`
* Improve `initctl show SVC` output.  Add service uptime, last ten SVC
  logs/history (historian), etc. Similar to `systemctl show SVC`
* [Solaris SMF][] (Service Management Facility) has some interesting
  features that may be well worth looking into adopting:
  
         svcs enable -rt svc:/network/ssh:default
  
  That is: (r)esolve dependencies and only enable ssh (t)emporary
  in the default runlevel when networking is available.
* `initctl add /sbin/service -n -- Service description`, for details
  see above [Solaris SMF][] featuer, issue #55 and issue #69,
  https://github.com/troglobit/finit/issues/69#issuecomment-287907610


Init
----

Finit in itself is not SysV Init compatible, it doesn't use `/etc/rc.d`
and `/etc/inet.d` scripts to boot the system.  However, a plausible way
to achieve compatibility would be to add a plugin to finit which reads
`/etc/inittab` to be able to start standard Linux systems.


Inetd
-----

* Add support for throttling connections.
* Optimize interface filtering by using socket filter.  The functions
  `inet_*_peek()` and `inetd_is_allowed()` used for interace filtering
  should be possible to rewrite as socket filters.
* Optimize HTTP/HTTPS inetd connections by adding basic support for the
  `inetd` variant `redir http/tcp@eth0 nowait [2345] 127.0.0.1:8080`,
  which would reduce the overhead of spawn the web server on each HTTP
  connection, yet still provide a means of filtering access per iface.


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


Documentation
-------------

* Write man pages for finit and `finit.conf`, steal from the excellent
  `pimd` man pages ...
* Add simple, *very* simple, `finit-simple.conf` example. To illustrate
  how close Finit3 still is to the original easy-to-use Finit0 :)


[libwdt]: http://www.wehavemorefun.de/fritzbox/Libwdt.so
[Fritz!Box source dump]: ftp://ftp.avm.de/fritz.box/fritzbox.fon_wlan_7170/x_misc/opensrc/
[Solaris SMF]: http://www.oracle.com/technetwork/articles/servers-storage-admin/intro-smf-basics-s11-1729181.html

<!--
  -- Local Variables:
  -- mode: markdown
  -- End:
  -->
