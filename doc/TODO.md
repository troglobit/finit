Goals of Finit
==============

Initially Finit aimed to be more than a processes monitor, acting as a
replacement for cron/at and inetd as well.  Over time, however, the true
value in the project turned out to be:

> A simple SysV init and systemd replacement, primarily for embedded
> systems, container, and dedicated server applications.


Misc TODOs
----------

Some of these are also registered in the issue tracker on GitHub.

* Refactor the .conf parser, parse user input in a separate process
* Move process monitor to separate process (same as parser?)
* Add new (complimentary) .svc file format to slowly migrate away
  from the very terse one-liner format currently used
* Allow running as non-pid1 => read .conf and RCSD form cmdline
* Add `finit.conf` support for UPS notification (SIGPWR) to start a task
  using, e.g. <sys/power/{ok,fail,low}> conditions.  More info in sig.c

  As of Finit v4.1 SIGPWR generates <sys/power/fail> condition and it
  is up to a task to check why and take appropriate action.
* Add `finit.conf` support for ctrl-alt-delete (SIGINT) and kbrequest,
  i.e. KeyboardSignal, (SIGWINCH) behavior.  Using conditions to a task,
  e.g, <sys/key/ctrlaltdel> and <sys/key/signal> like SIGPWR handling.

  As of Finit v4.1 SIGINT generates <sys/key/ctrlaltdel>, but there is
  no check if the signal actually originates from the kernel.  To get
  this verification, a new libuEv release is needed.  As of June 6 -21
  that functionality is not yet released.


Initctl
-------

* Add support for timed shutdown in Finit, including cancelled shutdown:

        shutdown -h 03:35 "foobar was here"

* Add support for JSON output, or similar, from `initctl show`, e.g.
  `initctl show --json`
* Add support for `-s` to redirect all output to syslog, for scripting


Init
----

Finit in itself is not SysV Init compatible, it doesn't use `/etc/rc.d`
and `/etc/init.d` scripts to boot the system.  However, a plausible way
to achieve compatibility would be to add a plugin to finit which reads
`/etc/inittab` to be able to start standard Linux systems.

