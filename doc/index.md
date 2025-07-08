Introduction
============

> Reverse engineered from the [EeePC fastinit][]  
> "gaps filled with frog DNA …"  
> — [Claudio Matsuoka][]

Finit is a process starter and [supervisor][1] designed to run as PID 1
on Linux systems.  It consists of a set of [plugins](plugins.md) and can
be set up using [configuration files](config/files.md).  Plugins start
at [hook points](plugins.md#hooks) and can run various set up tasks
and/or install event handlers that later provide runtime services, e.g.,
PID file monitoring, or [conditions](conditions.md).

Features include:

  * [Runlevels][5], defined per service
  * One-shot tasks, services (daemons), or [SysV init][4] start/stop scripts
  * Runparts and `/etc/rc.local` support
  * Process supervision similar to [systemd][]
  * Sourcing environment files
  * Conditions for network/process/custom dependencies
  * Readiness notification; PID files (native) for synchronizing system
    startup, support for systemd [sd_notify()][], or [s6 style][] too
  * Limited support for [tmpfiles.d(5)][] (no aging, attributes, or subvolumes)
  * Pre/Post script actions
  * Rudimentary [templating support](config/templating.md)
  * Tooling to enable/disable services
  * Built-in getty
  * Built-in watchdog, with support for hand-over to [watchdogd][]
  * Built-in support for Debian/BusyBox [`/etc/network/interfaces`][3]
  * Cgroups v2, both configuration and monitoring in `initctl top`
  * Plugin support for customization
  * Proper rescue mode with bundled `sulogin` for protected maintenance shell
  * Integration with [watchdogd][] for full system supervision
  * Logging to kernel ring buffer before `syslogd` has started, see the
    recommended [sysklogd][] project for complete logging integration
	and how to log to the kernel ring buffer from scripts using `logger`

For a more thorough overview, see the [Features](features.md) section.

> [!TIP]
> See [SysV Init Compatibility](config/sysv.md) for help to
> quickly get going with an existing SysV or BusyBox init setup.


Origin
------

This project is based on the [original finit][] by [Claudio Matsuoka][]
which was reverse engineered from syscalls of the [EeePC fastinit][].

Finit is developed and maintained by [Joachim Wiberg][] at [GitHub][6].
Please file bug reports, clone it, or send pull requests for bug fixes
and proposed extensions.



[1]:                https://en.wikipedia.org/wiki/Process_supervision
[3]:                https://manpages.debian.org/unstable/ifupdown2/interfaces.5.en.html
[4]:                https://en.wikipedia.org/wiki/Init
[5]:                https://en.wikipedia.org/wiki/Runlevel
[6]:                https://github.com/troglobit/finit
[systemd]:          https://www.freedesktop.org/wiki/Software/systemd/
[sd_notify()]:      https://www.freedesktop.org/software/systemd/man/sd_notify.html
[s6 style]:         https://skarnet.org/software/s6/notifywhenup.html
[tmpfiles.d(5)]:    https://www.freedesktop.org/software/systemd/man/tmpfiles.d.html
[EeePC fastinit]:   https://web.archive.org/web/20071208212450/http://wiki.eeeuser.com/boot_process:the_boot_process
[original finit]:   http://helllabs.org/finit/
[Claudio Matsuoka]: https://github.com/cmatsuoka
[Joachim Wiberg]:   https://troglobit.com
[watchdogd]:        https://troglobit.com/watchdogd.html
[sysklogd]:         https://github.com/troglobit/sysklogd/
