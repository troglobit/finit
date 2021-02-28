Goals of Finit
==============

Initially Finit aimed to be more than a processes monitor, acting as a
replacement for cron/at and inetd as well.  Over time, however, the true
value in the project turned out to be:

> A simple SysV init and systemd replacement, primarily for embedded
> systems, container, and dedicated server applications.


Near Future
-----------

* ... when inetd support has been removed, we can refactor the .conf
  parser, parse user input in a separate process, move process monitor
  to separate process (same as parser?), and possibly even ...
* ... add new (complimentary) .svc file format to slowly migrate away
  from the very terse one-liner format currently used.
* Allow running as non-pid1 => read .conf and RCSD form cmdline
* When a process dies, and Finit does not restart it, we should collect
  a `siginfo_t` from the SIGCHLD signal and supply to the user in case
  `initctl status <process>` is called to debug the issue.
* Add `finit.conf` support for UPS notification (SIGPWR) to start a task
  using, e.g. <sys/power/{ok,fail,low}> conditions.  More info in sig.c
* Add `finit.conf` support for ctrl-alt-delete (SIGINT) and kbrequest,
  i.e. KeyboardSignal, (SIGWINCH) behavior.  Using conditions to a task,
  e.g, <sys/key/ctrlaltdel> and <sys/key/signal> like SIGPWR handling.
* Write man pages for finit and `finit.conf`, steal from the excellent
  `pimd` man pages ...


General
-------

* Add support for timed shutdown in Finit, including cancelled shutdown,
  possibly using an at-job, see [crond section](#Crond) below
* Add support for JSON output, or similar, from `initctl show`, e.g.
  `initctl show --json`
* [Solaris SMF][] (Service Management Facility) has some interesting
  features that may be well worth looking into adopting:
  
         svcs enable -rt svc:/network/ssh:default
  
  That is: (r)esolve dependencies and only enable ssh (t)emporary
  in the default runlevel when networking is available.
* `initctl add /sbin/service -n -- Service description`, for details
  see above [Solaris SMF][] featuer, issue #55 and issue #69,
  https://github.com/troglobit/finit/issues/69#issuecomment-287907610
* Changing the name of a pidfile, `pid:[/path/]file.pid`, after a
  service has been started is not supported atm.  The previous name will
  not be (never) removed and the new name will not be created until the
  process has been stopped and started again.
* PID files created in subdirectories to `/run` is not supported right
  now inotify listeners should be automatically created (and removed)
  when a subdirectory is added (or removed).

[Solaris SMF]: http://www.oracle.com/technetwork/articles/servers-storage-admin/intro-smf-basics-s11-1729181.html


Init
----

Finit in itself is not SysV Init compatible, it doesn't use `/etc/rc.d`
and `/etc/init.d` scripts to boot the system.  However, a plausible way
to achieve compatibility would be to add a plugin to finit which reads
`/etc/inittab` to be able to start standard Linux systems.

