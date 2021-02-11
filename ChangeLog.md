Change Log
==========

All relevant changes are documented in this file.


[3.2][UNRELEASED] - 2021-03-xx
------------------------------

Major bug fix release.  New features include cgroups and a new progress!

  https://twitter.com/b0rk/status/1214341831049252870?s=20

> **Note:** deprecation warning, this is likely the last release with
>           built-in inetd support!  Current plan is to rip it out at
>           the start of the next release cycle.  It *may* return as
>           a separate stand-alone daemon.

### Changes
* Introducing Finit progress 𝓜𝓸𝓭𝓮𝓻𝓷
* Incompatible `configure` script changes, i.e., you must give proper
  path arguments to the script, no more guessing just GNU defaults.
  There are examples in the documentation and the `contrib/` section
* Change service conditions from the non-obvious `<svc/path/to/foo>` to
  `<pid/foo:id>`.  This to hopefully make it clear that one service's
  'pid:!foo' pidfile is another service's `<pid/foo>` condition
* Major refactor of Finit's `main()` function to be able to start the
  event loop earlier.  This also facilitated factoring out functionality
  previously hard-coded in Finit, e.g., starting the bundled watchdogd,
  various distro packed udevd and other hotplugging tools
* Support for `sysv` start/stop scripts as well as monitoring forking
  services, stared using `sysv` or `service` stanza
* Support for custom `kill:DELAY`, default 3 sec.
* Support for custom `halt:SIGNAL`, default SIGTERM
* Support for tracking custom PID files, using `pid:!/path/to/foo.pid`,
  useful with new `sysv` or `service` which fork to background
* Support starting run/task/services without absolute path, trust `$PATH`
* Add support for `--disable-doc` and `--disable-contrib` to speed up
  builds and work around issue with massively parallel builds
* Add support for `@console` also for external getty
* Add `-b`, batch mode, for non-interactive use to `initctl`
* Prefer udev to handle `/dev/` if mdev is also available
* Redirect dbus daemon output to syslog
* Set `$SHELL`, like `$PATH`, to a sane default value, needed by BusyBox
* Finit no longer automatically reloads its `*.conf` files after running
  `/etc/rc.local` or run-parts.  Use `initctl reload` instead.
* `initctl` without an argument or option now defaults to list services
* Convert built-in watchdog daemon to standalone mini watchdogd, issue #102
* Improved watchdog hand-over, now based on `svc_t` and not PID
* Extended bootstrap, runlevel S, timeout: 10 --> 120 sec. before services
  not allowed in the runtime runlevel are unconditionally stopped
* Removed `HOOK_SVC_START` and `HOOK_SVC_LOST`, caused more problems
  than they were worth.  Users are encouraged to use accounting instead
* Skip displaying "Restarting ..." progress for bootstrap processes
* Added a simple work queue mechanism to queue up work at boot + runtime
  - Postpone deletion of `svc_t` until any `SIGKILL` timer has elapsed
  - As long as a stepped service changes state we queue another step all
    event, because services may depend on each other
* Require new libuEv API: `uev_init1()` to reduce event cache so that
  the kernel can invalidate deleted events before enqueing to userspace
* Rename `hwclock.so` plugin to `rtc.so` since it now is stand-alone
  from the `hwclock` tool.  Note: the kernel can also be set to load
  and store RTC to/from system clock at boot/halt as well, issue #110
* New plugin to support cold plugging devices, auto-loading of modules
  at boot.  Detects required modules by reading `/sys/devices/*`
* New plugin for `/etc/modules-load.d/` by Robert Andersson, Atlas Copco
* New `name:foo` support for services, by Robert Andersson, Atlas Copco
* New `manual:yes` support for services, by Robert Andersson, Atlas Copco
* New `log:console` support for services, by Robert Andersson, Atlas Copco
* Support for `:ID` as a string, by Jonas Johansson, Westermo
* Support for auto-reload, instead of having to do `initctl reload`,
  when a service configuration has changed.  Disabled by default, but
  can be enabled with `./configure --enable-auto-reload`
* Support for logging security related events, e.g., runlevel change,
  star/stop or failure to start services, by Jonas Holmberg, Westermo
* Mount devtpts with recommended `ptxmode=0666`
* Mount /run tmpfs with nosuid,nodev,noexec for added security
* Support for `console` as alias for `@console` in tty stanzas
* Drop `--enable-rw-roots` configure option, use `rw` for your `/`
  partition in `/etc/fstab` instead to trigger remount at boot
* Drop default tty speed (38400) and use 0 (kernel default) instead
* Make `:ID` optional for real, use NULL/zero internally and only
  force :ID (numbered) for inetd services, this allows ...
* Handle use-cases where multiple services share the same PID filem
  and thus the same condition path, e.g. different instances for
  different runlevels.  Allow custom condition path with `name:foo`
  syntax, creates conditions w/o a path, and ...
* Always append `:ID` qualifier to conditions if set for a service
* The IPC socket has moved from `/run/finit.sock` to `/run/finit/socket`
  officially only supported for use by the `initctl` tool
* Improved support for modern `/etc/network/interfaces`, which has
  include statements.  No more native `ifup` of individual interfaces,
  Finit now calls `ifup -a`, or `ifdown -a`, delegating all details to
  the operating system.  Also, this is now done in the background, by
  popular request

### Fixes

* Fix #96: Start udevd as a proper service
* Ensure we track run commands as well as task/service, once per runlevel
* Fix #98: FTBFS with `--disable-inetd`
* Make sure to unblock UDP inetd services when connection terminates.
  Regression introduced in v3.1
* Ensure run/tasks also go to stopping state on exit, like services,
  otherwise it is unnecessarily hard to restart them
* Fix #99: Do not try to `SIGKILL` inetd services, they are not backed
  by a PID.  This caused a use after free issue crashing finit.  Found
  and fixed by Tobias Waldekranz, Westermo
* Fix missing OS/Finit title bug, adds leading newline before banner
* Remove "Failed connecting to watchdog ..." error message on systems
  that do not have a watchdog
* Fix #100: Early condition handling may not work if `/var/run` does
  not yet exist (symlink to `/run`).  Added compat layer for access
* Fix #103: Register multiple getty if `@console` resolves to >1 TTY,
* Fix #105: Only remove /etc/nologin when moving from runlevel 0, 1, 6
* Fix #106: Don't mark inetd connections for deletion at .conf reload.
  Fixed by Jonas Johansson, Westermo
* Fix #107: Stop spawned inetd conncections when stopping inetd service.
  Fixed by Jonas Johansson, Westermo
* Fix #109: Support for PID files in sub-directories to `/var/run`
* Handle rename of PID files, by Robert Andersson, Atlas Copco
* Fix #111: Only restart inetd services when necessary.  E.g., if the
  listening interface is changed.  Only stop established connections
  which are no longer allowed, i.e. do not touch already allowed
  established connections.  Fixed by Jonas Johansson, Westermo
* Fix #120: Redirect `stdin` to `/dev/null` for services by default
* Fix #122: Switch to `nanosleep()` to achieve "signal safe" sleep,
  fixed by Jacques de Laval, Westermo
* Fix #124: Lingering processes in process group when session leader
  exits.  E.g., lingering `logit` processes when parent dies
* Fix: update inetd service args on config change.  Found and fixed by
  Petrus Hellgren, Westermo
* Fix service name matching, e.g. for condition handling, may match with
  wrong service, by Jonas Holmberg, Westermo
* Run all run-parts scripts using `/bin/sh -c foo` just like the standard
  run-parts tool.  Found by Magnus Malm, Westermo
* Fix `initctl [start | restart]`, should behave the same for services
  that have crashed.  Found by Mattias Walström, Westermo
* Wait for bootstrap phase to complete before cleaning out any bootstrap
  processes that have stopped, they may be restarted again
* Reassert condition when an unmodified run/task/service goes from
  WAITING back to RUNNING again after a reconfiguration event.  
  Found and fixed by Jonas Johansson, Westermo
* Restore Ctrl-D and Ctrl-U support in built-in getty
* Remove service condition when service is deleted
* Fix C++ compilation issues, by Robert Andersson, Atlas Copco
* Build fixes for uClibc
* Provide service description for built-in watchdog daemon
* Fix #138: Handle `SIGPWR` like `SIGSUR2`, i.e., power off the system
* Drop the '%m' GNUism, for compat with older musl libc


[3.1][] - 2018-01-23
--------------------

Improvements to `netlink.so` plugin, per-service `rlimit` support,
improved integration with `watchdogd`, auto-detect TTY console.  Much
improved debug, rescue and logging support.  Also, many fixes to both
big and small issues, most notably in the condition handling, which no
longer is sensitive to time skips.

This version requires at least libuEv v2.1.0 and libite v2.0.1

### Changes

* Support for more kernel command line settings:
  - splash, enable boot progress
  - debug, like `--debug` but also enable kernel debug
  - single, single user mode (no network)
  - rescue, new rescue mode
* Support for `IFF_RUNNING` to netlink plugin => `net/IFNAME/running`
* Support for restarting `initctl` API socket on `SIGHUP`
* Greatly updated `initctl status <JOB|NAME>` command
* Support for `rlimit` per service/run/task/inetd/tty, issue #45
* Support for setting `hard` and `soft` rlimit for a resource at once
* Support for auto-detecting serial console using Linux SysFS, the new
  `tty @console` eliminates the need to keep track of different console
  devices across embedded platforms: `/dev/ttyS0`, `/dev/ttyAMA0`, etc.
* Add TTY `nologin` option.  Bypasses login and skips immediately to a
  root shell.  Useful during board bringup, in developer builds, etc.
* Support for calling run/tasks on Finit internal HOOK points, issue #18
* Removed support for long-since deprecated `console DEV` setting
* Cosmetic change to login, pressing enter at the `Press enter to ...`
  prompt will now replace that line with the login issue text
* Calling `initctl` without any arguments or options now defaults to
  show status of all enabled services, and run/task/inetd jobs
* Cosmetic change to boot messages, removed `Loading plugins ...`, start
  of inetd services, and `Loading configuration ...`.  No end user knows
  what those plugins and configurations are, i.e. internal state+config
* Change kernel WDT timeout (3 --> 30 sec) for built-in watchdog daemon
* Advise watchdog dawmon on shutdown and reboot using `SIGPWR` and then
  `SIGTERM`.  It is recommended the daemon start a timer on the first
  signal, in case the shutdown process somehow hangs.
* Handle `/etc/` OverlayFS, reload /etc/finit.d/*.conf after `mount -a`
* initctl: Add support for printing previous runlevel
* initctl: Support short forms of all commands
* initctl: Support for `initctl touch <CONF>` to be used with `reload`
* initctl: Improved output of `initctl show <SVC>`
* Support reloading `/etc/finit.conf`.  The main finit.conf file
  previously did not support reloading at runtime, as of v3.1 all
  configuration directives supported in `/etc/finit.d/*.conf` are now
  supported in `/etc/finit.conf`
* Change `.conf` dependency + reload handling.  Finit no longer relies
  on mtime of `.conf` files, instead an inotify handler tracks file
  changes for time *insensitive* dependency tracking
* Change condition handling to not rely on mtime but a generation id.
* New configure script option `--enable-redirect` to automatically
  redirect `stdout` and `stderr` of all applications to `/dev/null`
* New `pid` sub-option to services when a service does not create a PID
  file, or when the PID file has another name.  Issue #95
* Greatly improved `log` sub-option to service/run/tasks, selectively
  redirect `stdout` and `stderr` using the new tool `logit` to either
  syslog or a logfile.  Issue #44
* Support for automatic log rotation of logfiles created by `log`
  option.  Use `configure --disable-logrotate` on systems with a
  dedicated log rotation service.  Issue #44
* Support for disabling service/run/task progress with empty ` --`
  description.  Note: no description separator gives a default desc.
* Create `/etc/mtab` symlink if missing on system (bootmisc plugin)
* New hook: `hook/mount/post` runs after `mount -a` but before the
  `hook/mount/all`, where `bootmisc.so` runs.  This provides the
  possibility of running a second stage mount command before files in
  `/run` and similar are created
* Skip `gdbserver` when unleashing the grim reaper at shutdown
* Distribute and install `doc/` and `contrib/` directories

### Fixes

* Reset TTY before restarting it.  A program may manipulate the TTY in
  various ways before the user logs out, Finit needs to reset the TTY to
  a sane state before restarting it.  Issue #84
* On .conf parse errors, do *not* default to set TTY speed 38400, reuse
  current TTY speed instead
* Fix run/tasks, must be guaranteed to run once per declared runlevel.
  All run/tasks on `[S]` with a condition `<...>` failed to run.  Finit
  now tracks run/tasks more carefully, waiting for them to finish before
  switching to the configured runlevel at boot.  Issue #86
* Allow inetd services to be registered with a unique ID, e.g. `:161`,
  issue #87.  Found by Westermo
* inetd: drop UDP packets from blocked interfaces, issue #88
* Handle obscure inter-plugin dependency issue by calling the netlink
  plugin before the pidfile plugin on `HOOK_SVC_RECONF` events
* Handle event loop failure modes, issue found by Westermo
* Handle API socket errors more gracefully, restart socket
* Do not attempt to load kernel modules more than once at bootstrap
* Remove reboot symlinks properly on uninstall
* Fix regression in condition handling, reconf condition must be kept
  as a reference point to previous reconfiguration, or bootstrap.
* Fix nasty race condition with internal service stop, abort kill if the
  service has already terminated, otherwise we may do `kill(0, SIGKILL)`
* Fix reconfiguration issue with (very quick) systems that don't have
  highres timers
* Fix formatting of runlevel string in `initctl show`
* Allow running `initctl` with `STDOUT` redirected
* Fix regression in `initctl start/stop <ID>`, using name worked not id
* Fix error handling in `initctl start/stop` without any argument
* Fix issue #81 properly, remove use of SYSV shm IPC completely.  Finit
  now use the API socket for all communication between PID 1 and initctl
* Fix segfault on x86_64 when started with kernel cmdline `--debug`
* Normalize condition paths on systems with `/run` instead of `/var/run`


[3.0][] - 2017-10-19
--------------------

Major release, support for conditions/dependencies between services,
optional built-in watchdog daemon, optional built-in getty, optional
built-in standard inetd services like echo server, rdate, etc.

Also, native support for Debian/BusyBox `/etc/network/interfaces`,
overhauled new configure based build system, logging to `/dev/kmsg`
before syslogd has started, massively improved support for Linux
distributions.

### Changes

* Added basic code of conduct covenant to project
* Added contribution guidelines document
* Removed `finit.conf` option `check DEV`, replaced entirely by automated
  call to `fsck` for each device listed in `/etc/fstab`
* Removed deprecated and confusing `startx` and `user` settings.  It is
  strongly recommended to instead use xdm/gdb/lightdm etc.
* Add support for `initctl log <SVC>`, shows last 10 lines from syslog
* Add `initctl cond dump` for debugging conditions
* Ensure plugins always have a default name, file name
* Reorganization, move all source files to a `src/` sub-directory
* Add support for `initctl <list|enable|disable> <SVC>`, much needed by
  distributions.  See [doc/distro.md](doc/distro.md) for details
* Remove `UNUSED()` macro, mentioned here because it may have been used
  by external plugin developers.  Set `-Wno-unused-parameter` instead
* New table headings in `initctl`, using `top` style inverted text
* Allow `initctl show` to use full screen width for service descriptions
* New `HOOK_BANNER` for plugins to override the default `banner()`
* Allow loading TTYs from `/etc/finit.d`
* Improvements to built-in getty, ignore signals like `SIGINT`,
  `SIGHUP`, support Ctrl-U to erase to beginning of line
* Add TTY `nowait` and `noclear` options
* Allow using both built-in getty and external getty:  

        tty [12345] /dev/ttyAMA0    115200              noclear vt220
        tty [12345] /sbin/getty  -L 115200 /dev/ttyAMA0 vt100
        tty [12345] /sbin/agetty -L ttyAMA0 115200      vt100 nowait

* Silent boot is now the default, use `--enable-progress` to get the old
  Finit style process progress.  I.e., `--enable-silent` is no more
* Support for `configure --enable-emergency-shell`, debug-only mode
* Support for a fallback shell on console if none of the configured TTYs
  can be started, `configure --enable-fallback-shell`
* All debug messages to console when Finit `--debug` is enabled
* Prevent login, by touching `/etc/nologin`, during runlevel changes
* A more orderly shutdown.  On reboot/halt/poweroff Finit now properly
  goes to runlevel 0/6 to first stop all processes.
* Perform sync before remounting as read-only, at shutdown
* Clean up `/tmp`, `/var/run`, and `/var/lock` at boot on systems which
  have these directories on persistent storage
* Call udev triggers at boot, on systems with udev
* Add missing `/var/lock/subsys` directory for dbus
* Add support for `poweroff`
* Add support for a built-in miniature watchdog daemon
* Remove GLIBC:isms like `__progname`
* Manage service states based on user defined conditions
* Manage dependencies between services, w/ conditions (pidfile plugin)
* Manage service dependencies on network events (netlink plugin)
* Support for dynamically reloading Finit configuration at runtime
* Refactor to use GNU configure and build system
* New hooks for for detecting lost and started services (lost plugin)
* External libraries, libuEv and libite, now build requirements
* Early logging support to `/dev/kmsg` instead of console
* Support for redirecting stdout/stderr of services to syslog
* Support for managing resource limits for Finit and its processes
* Add optional built-in inetd services: echo server, chargen, etc.
* Add simple built-in getty
* Greatly improved accounting support, both UTMP and WTMP fixes+features
* Improved udev support, on non-embedded systems
* Improved shutdown and file-system unmount support (Debian)
* Support SysV init `/etc/rc.local`
* Inetd protection against UDP looping attacks
* Support systems with `/run` instead of `/var/run` (bootmisc plugin)
* Adopted BusyBox init signals for halt/reboot/poweroff
* SysV init compat support for reboot (setenv)
* Compat support for musl libc
* Add OpenRC-like support for sysctl.d/*.conf
* Add support for Debian/BusyBox `/etc/network/interfaces`
* Add support for running fsck on file systems in `/etc/fstab`
* Added example configs + HowTos for Debian and Alpine Linux
  to support latest releases of both distributions
* Lots of documentation updates

### Fixes

* Fix race-condition at configuration reload due to too low resolution.  
  Thanks to Mattias Walström, Westermo
* Fix to handle long process (PID) dependency chains, re-run reconf
  callback until no more applications are in flux.  
  Thanks to Mattias Walström, Westermo
* Clear `reconf` condition when `initctl reload` has finished
* Skip automatic reload of `/etc/finit.d/*.conf` files when changing
  runlevel to halt or reboot
* Fix issue #54: Allow halt and poweroff commands even if watchdog is enabled
* Fix issue #56: Check existence of device before trying to start getty
* Fix issue #60: `initctl` should display error and return error code
  for non-existing services should the operator try to start/stop them.
* Fix issue #61: Reassert `net/*` conditions after `initctl reload`
* Fix issue #64: Skip `fsck` on already mounted devices
* Fix issue #66: Log rotate and gzip `/var/log/wtmp`, created by Finit
* Fix issue #72: Check `ifup` exists before trying to bring up networking,
  also set `$PATH` earlier to simplify `run()` et al -- no longer any need
  to use absolute paths for system tools called from Finit.  Thanks to crazy
* Fix issue #73: Remove double `ntohl()` in inetd handling, prevents matches.  
  Thanks to Petrus Hellgren, Westermo
* Fix issue #76: Reap zombie processes in emergency shell mode
* Fix issue #80: FTBFS on Arch Linux, missing `stdarg.h` in `helpers.h`,
  thanks to Jörg Krause
* Fix issue #81: Workaround for systems w/o SYSV shm IPC support in kernel
* Always collect bootstrap-only tasks when done, we will never re-run them.
  Also, make sure to never reload bootstrap-only tasks at runtime
* Remove two second block (!) of Finit when stopping TTYs


[2.4][] - 2015-12-04
--------------------

Bug fix release.

### Changes

* Add support for status/show service by `name:id`
* Enforce terse mode after boot, if verbose mode is disabled
* Reenable verbose mode at reboot, if disabled at boot
* Update section mentioning BusyBox getty
* Update debugging documentation
* Allow debug to override terse mode
* Revert confusing change in service state introduced in [v2.3][2.3].
  As of [v2.4][2.4] services are listed as "halted" and "stopped", when
  they have been halted due to a runlevel changed or stopped by the
  user, respectively.

### Fixes

* Fix system freeze at reconfiguration.  Changed services that
  all support `SIGHUP` caused a freeze due to Finit waiting for
  them to stop.
* Make sure to start and/or `SIGHUP` services after reconfiguration
  when there was no services to stop.


[2.3][] - 2015-11-28
--------------------

Bug fix release.

### Changes

* Add support for stop/start/restart/reload service by `name:id`
* Refactor service status listed in `initctl show`, show actual status

### Fixes

* Remove bootstrap-only tasks/services when leaving runlevel 'S'
* Fix reference counting issue with already stopped and removed services
  when the user performs `initctl reload` to change system configuration
* Revert semantic change in behavior of `initctl restart`: users expect
  service to be stopped/started, not reloaded with `SIGHUP` even if the
  service supports it
* Fix `NULL` pointer dereference causing kernel panic when user calls
  `initctl reload` after change of system configuration
* Fix column alignment in output of `initctl show` for services not in
  current runlevel


[2.2][] - 2015-11-23
--------------------

Lots of fixes to handle static builds, but also fixes for dynamic event
handling and reconfiguration at runtime.

### Changes

* Upgrade to libuEv v1.2.4, to handle static builds
* Upgrade to libite (LITE) v1.2.0, to handle static builds
* Clarify how to select different plugins with the configure script
* Improve urandom plugin for embedded systems w/o random seed
* Add `--debug` flag to `initctl`
* The runlevels listed for services in `initctl show` now hightlight the
  active runlevel.
* Clarify in the README and in `initctl help` that the GW event to
  listen for in service declarations is `GW:UP`

### Fixes

* Build fixes for `configure --disable-inetd`
* Fixed issue #14: Improved support for static Finit builds
* Misc. fixes to silence warnings when building a static Finit
* Default to register services as `SIGHUP`'able, regression in v2.0
* Call `HOOK_SVC_RECONF` only when all processes have been stopped
* On reload/reconf we must wait for all services to stop first
* Only trigger on events that matches the service's specification,
  fix by Tobias Waldekranz


[2.1][] - 2015-10-16
--------------------

### Changes

* Add hook point for fstab mount failure
* Set hostname on dynamic reload
* Upgrade to libite v1.1.1

### Fixes

* Fix service callback coredump checks and simplify callback exit
* Do not use `-Os` use `-O2` as default optimization level.  Many cross
  compiler toolchains are known to have problems with `-Os`
* Do not allow build VERSION to be overloaded by an environment variable
* Fix too small MAX arguments and too few argments in `svc_t` for
  reading currently running services with `initctl show`
* Unblock blocked signals after forking off a child


[2.0][] - 2015-09-20
--------------------

Support for multiple instances and event based services, as well as the
introduction of an `initctl` tool.

**Note:** Incompatible change to syntax for custom `inetd` services,
  c.f. Finit v[1.12][].

### Changes
* The most notable change is the support for multiple instances.  A must
  have when running multiple DHCP clients, OpenVPN tunnels, or anything
  that means using the same command only with different arguments.  Now
  simply add a `:ID` after the `service` keyword, where `ID` is a unique
  instance number for that service.

        service :1 [2345] /sbin/httpd -f -h /http -p 80   -- Web server
        service :2 [2345] /sbin/httpd -f -h /http -p 8080 -- Old web server

* Another noteworthy new feature is support for starting/stopping
  services on Netlink events:

        service :1 [2345] <!IFUP:eth0,GW> /sbin/dropbear -R -F -p 22 -- SSH daemon

  Here the first instance `:1` of the SSH daemon is declared to run in
  runlevels 2-5, but only if eth0 `IFUP:eth0` is up and a gateway `GW`
  is set.  When the configuration changes, a new gateway is set, or if
  somehow a new `IFUP` event for eth0 is received, then dropbear is not
  SIGHUP'ed, but instead stop-started `<!>`.  The latter trick applies
  to all services, even those that do not define any events.
* Support for reloading `*.conf` files in `/etc/finit.d/` on SIGHUP.
  All `task`, `service` and `run` statements can be used in these .conf
  files.  Use the `telinit q` command, `initctl reload` or simply send
  `SIGHUP` to PID 1 to reload them.  Finit automatically does reload of
  these `*.conf` files when changing runlevel.
* Support for a modern `initctl` tool which can stop/start/reload and
  list status of all system services.  Also, the old client tool used
  to change runlevel is now also available as a symlink: `telinit`.

        initctl [-v] <status|stop|start|reload|restart> [JOB]

* Add concept of "jobs".  This is a unique identifier, composed of a
  service and instance number, `SVC:ID`

        initctl <stop|start|reload|restart> JOB

* Support for *deny filters* in `inetd` services.

        inetd service/proto[@iface,!iface,...] </path/to/cmd | internal[.service]>

  Internal services on a custom port must use the `internal.service`
  syntax so Finit can properly bind the inetd service to the correct
  plugin.  Here follows a few examples:

        inetd time/udp                    wait [2345] internal                -- UNIX rdate service
        inetd time/tcp                  nowait [2345] internal                -- UNIX rdate service
        inetd 3737/tcp                  nowait [2345] internal.time           -- UNIX rdate service
        inetd telnet/tcp@*,!eth1,!eth0, nowait [2345] /sbin/telnetd -i -F     -- Telnet service
        inetd 2323/tcp@eth1,eth2,eth0   nowait [2345] /sbin/telnetd -i -F     -- Telnet service
        inetd 222/tcp@eth0              nowait [2345] /sbin/dropbear -i -R -F -- SSH service
        inetd ssh/tcp@*,!eth0           nowait [2345] /sbin/dropbear -i -R -F -- SSH service

  Access to telnet on port `2323` is only possible from interfaces
  `eth0`, `eth1` and `eth2`.  The standard telnet port (`23`) is
  available from all other interfaces, but also `eth2`.  The `*`
  notation used in the ssh stanza means *any* interface, however, here
  `eth0` is not allowed.

  **NOTE:** This change breaks syntax compatibility with Finit v[1.12].
* Support for a more user-friendly configure script rather than editing
  the top Makefile, or setting environment variables at build time.
* Support for building Finit statically, no external libraries.  This
  unfortunately means that some plugins cannot be built, at all.
  Big thanks goes to James Mills for all help testing this out!
* Support for disabling the built-in inetd server with `configure`.
* Support for two new hook points: `HOOK_SVC_RECONF` and
  `HOOK_RUNLEVEL_CHANGE`.  See the source for the exact location.
* The `include <FILE>` option now needs an absolute path to `FILE`.

### Fixes
* Rename `patches/` to `contrib/` to simplify integration in 3rd party
  build systems.
* Fix for unwanted zombies ... when receiving SIGCHLD we must reap *all*
  children.  We only receive one signal, but multiple processes may have
  exited and need to be collected.


[1.12][] - 2015-03-04
---------------------

The inetd release.

### Changes
* Add support for built-in inetd super server -- launch services on
  demand.  Supports filtering per interface and custom Inet ports.
* Upgrade to [libuEv] v1.1.0 to better handle error conditions.
* Allow mixed case config directives in `finit.conf`
* Add support for RFC 868 (rdate) time plugin, start as inetd service.
* Load plugins before parsing `finit.conf`, this makes it possible to
  extend finit even with configuration commands.  E.g., the `time.so`
  plugin must be loaded for the `inetd time/tcp internal` service to be
  accepted when parsing `finit.conf`.
* Slight change in TTY fallback behavior, if no TTY is listed in the
  system `finit.conf` first inspect the `console` setting and only if
  that too is unset fall back to `/bin/sh`
* When falling back to the `console` TTY or `/bin/sh`, finit now marks
  this fallback as console.  Should improve usability in some use cases.

### Fixes
* Revert "Use vfork() instead of fork() before exec()" from v1.11.  It
  turned out to not work so well after all.  For instance, launching
  TTYs in a background process completely blocked inetd services from
  even starting up listening sockets ... proper fork seems to work fine
  though.  This is the casue for *yanking* the [1.11] release, below.
* Trap segfaults caused by external plugins/callbacks in a sub-process.
  This prevents a single programming mistake in by a 3rd party developer
  from taking down the entire system.
* Fix Coverity CID 56281: dlopen() resource leak by storing the pointer.
  For the time being we do not support unloading plugins.
* Set hostname early, so bootstrap processes like syslog can use it.
* Only restart *lost daemons* when recovering from a SIGSTOP/norespawn.


[1.11][] - 2015-01-24 [YANKED]
------------------------------

The [libuEv] release.

**Note:** This release has been *yanked* from distribution due to a
  regression in launching background processes and TTY's.  Fixed in
  Finit v[1.12].

### Changes
* Now using the asynchronous libuEv library for handling all events:
  signals, timers and listening to sockets or file descriptors.
* Rename NEWS.md --> CHANGELOG.md, with symlinks for `make install`
* Attempt to align with http://keepachangelog.com/ for this file.
* Travis CI now only invokes Coverity Scan from the 'dev' branch.  This
  means that all development, except documentation updates, must go into
  that branch.

### Fixes
* Fix bug with finit dying when no `tty` is defined in `finit.conf`, now
  even the fallback shell has control over its TTY, see fix in GIT
  commit [dea3ae8] for this.


[1.10][] - 2014-11-27
---------------------

Major bug fix release.

### Changes
* Project now relies on static code analysis from Coverity, so this
  release contains many serious bug fixes.
* Convert to use Markdown for README, NEWS and TODO.
* Serious update to README and slight pruning of finished TODO items.

### Fixes
* Fix serious file descriptor and memory leaks in the following
  functions.  In particular the leaks in `run_interactive()` are very
  serious since that function is called every time a service is started
  and/or restarted!  For details, see the GIT log:
  * `helpers.c:run()`
  * `helpers.c:run_interactive()`
  * `helpers.c:set_hostname()`
  * `helpers.c:procname_kill()`
* `svc.c:svc_start()`: Fix swapped arguments to dup2() and add close(fd)
  to prevent descriptor leak.
* `svc.c:svc_start()`: Fix out of bounds write to local stack variable,
  wrote off-by-one outside array.
* Several added checks for return values to `mknod()`, `mkdir()`,
  `remove()`, etc.


[1.9][] - 2014-04-21
--------------------

### Changes
* Add support for an include directive to `.conf` files
* Fallback to `/bin/sh` if user forgets tty setting
* Initial support for restarting lost services during `norespwan`

### Fixes
* Bug fixes, code cleanup
* Handle `SIGHUP` from service callback properly when switching runlevel
* Misc. major (memleak) and minor fixes and additions to `libite/lite.h`


[1.8][] - 2013-06-07
--------------------

### Changes
* Support for runlevels, with a bootstrap runlevel 'S'
* Support for saving previous and current runlevel to UTMP
* Support for new `finit.conf` commands: run, task, and runlevel
* Support for tty and console commands in `finit.conf`, like services
  but for launching multiple getty logins
* New tty plugin to monitor TTYs coming and going, like USB TTYs

### Fixes
* Bugfixes to libite


[1.7][] - 2012-10-08
--------------------

### Changes
* Show `__FILE__` in `_d()` debug messages, useful for plugins with
  similarily named callbacks. Also, only in debug mode anyway
* Make sure to cleanup recorded PID when a service is lost.  Required by
  service plugins for their callbacks to work.
* Only clear screen when in verbose mode. Maybe this should be removed
  altogether?

### Fixes
* Bugfix: Do not `free()` static string in `finit.conf` parser


[1.6][] - 2012-10-06
--------------------

### Changes
* Skip `.` and `..` in plugin loader and display error when failing to
  load plugins
* Support for overriding `/etc/finit.d` with `runparts DIR` in
  `finit.conf`
* Revoke support for starting services not starting with a slash.
* Prevent endless restart of non-existing services in `finit.conf`
* Support for sysvinit style startstop scripts in `/etc/finit.d`

### Fixes
* Minor fix to alsa-utils plugin to silence on non-existing cards


[1.5][] - 2012-10-03
--------------------

### Changes
* Use bootmisc plugin to setup standard FHS 2.3 structure in `/var`
* Added `FLOG_WARN()` syslog macro, for plugins
* Add plugin dependency resolver. Checks `plugin_t` for `.depends`


[1.4][] - 2012-10-02
--------------------

### Changes
* Start refactoring helpers.c into a libite.so (-lite).  This means
  other user space applications/daemons can make use of the neat toolbox
  available in finit
* Use short-form -s/-w -u to work with BusyBox hwclock as well
* Use RTLD_GLOBAL flag to tell dynamic loader to load dependent .so
  files as well.  Lets other plugins use global symbols.
* Greatly simplify svc hook for external plugins and cleanup plugin API.
* And more ... see the GIT log for more details.

### Fixes
* Fix I/O plugin watcher and load plugins earlier for a new hook


[1.3][] - 2012-09-28
--------------------

### Changes
* Cleanup public plugin API a bit and add new pid/pidfile funcs
* Add plugin hook to end of service startup
* Remove finit.h from svc.h, plugins should not need this.
* Move utility macros etc. to helpers.h
* Make `finit.h` daemon internal, only
* Move defines of FIFO, conf and rcS.d to Makefile => correct paths
* Add support for installing required headers in system include dir
* Better support for distributions and packagers with install-exec,
  install-data, and install-dev targets in Makefile.  Useful if you want
  to call targets with different `$DESTDIR`!
* Makefile fixes for installation, paths encoded wrong
* Strip binaries + .so files, support for `$(CROSS)` toolchain strip
* Default install is now to `/sbin/finit` and `/usr/`
* Note change in `$PLUGIN_DIR` environemnt variable to `$plugindir`


[1.2][] - 2012-09-27
--------------------

### Changes
* Update README with section on building and enviroment variables

### Fixes
* Fix installation paths encoded in finit binary


[1.1][] - 2012-09-27
--------------------

### Changes
* Rename signal.[ch]-->sig.[ch] to avoid name clash w/ system headers

### Fixes
* Build fixes for ARM eabi/uClibc


[1.0][] - 2012-09-26
--------------------

### Changes
* New plugin based system for all odd extensions
* New service monitor that restarts services if they die
* New maintainer at GitHub http://github.com/troglobit/finit
* Add standard LICENSE and AUTHORS files
* New focus: embedded systems and small headless servers


[0.6][] - 2010-06-14
--------------------

* Don't start consolekit manually, dbus starts it (rtp)
* Unmount all filesystems before rebooting
* Disable `USE_VAR_RUN_RESOLVCONF` for Mandriva
* Unset terminal type in Mandriva before running X
* Remove extra sleep in finit-alt before calling services.sh (caio)


[0.5][] - 2008-08-21
--------------------

* Add option to start dbus and consolekit before the X server
* finit-alt listens to `/dev/initctl` to work with `reboot(8)` (smurfy)
* Write runlevel to utmp, needed by Printerdrake (Pascal Terjan)
* Fix ownership of `/var/run/utmp` (reported by Pascal Terjan)
* Remove obsolete code to load AGP drivers
* Conditional build of `/etc/resolveconf/run` support
* Add support to `/var/run/resolvconf` in Mandriva (blino)


[0.4][] - 2008-05-16
--------------------

* Default username for finit-alt configurable in Makefile
* Create loopback device node in finit-alt (for squashfs)
* Add option to use built-in run-parts instead of `/bin/run-parts`
* Ignore signal instead of setting to an empty handler (Metalshark)
* Handle pam_console permissions in finit-alt for Mandriva
* Add services.sh example and nash-hotplug patch for Mandriva
* Mount `/proc/bus/usb` in Mandriva
* Add runtime debug to finit-alt if finit_debug parameter is specified
* Read configuration from `/etc/finit.conf`
* Run getty with openvt on the virtual terminal


[0.3][] - 2008-02-23
--------------------

* Change poweroff method to `reboot(RB_POWER_OFF)` (Metalshark)
* Remove duplicate `unionctl()` reimplementation error
* Fix string termination in path creation
* Mount `/var/lock` and `/var/run` as tmpfs


[0.2][] - 2008-02-19
--------------------

* Replace `system("touch")` with `touch()` in finit-mod (Metalshark)
* Disable `NO_HCTOSYS` by default to match stock Eeepc kernel
* Drop `system("rm -f")` to clean `/tmp`, its a fresh mounted tmpfs
* Write ACPI sleep state to `/sys/power/state` instead of
  `/proc/acpi/sleep` (Metalshark)
* Use direct calls to set loopback instead of `system("ifconfig")`
* Replace `system("cat")` and `system("dd")` with C implementation
* Moved finit-mod and finit-alt helpers to `helpers.c`
* Replace `system("echo;cat")` to draw shutdown splash with C calls


0.1 - 2008-02-14
----------------

* Initial release

[UNRELEASED]: https://github.com/troglobit/finit/compare/3.1...HEAD
[3.2]: https://github.com/troglobit/finit/compare/3.1...3.2
[3.1]: https://github.com/troglobit/finit/compare/3.0...3.1
[3.0]: https://github.com/troglobit/finit/compare/2.4...3.0
[2.4]: https://github.com/troglobit/finit/compare/2.3...2.4
[2.3]: https://github.com/troglobit/finit/compare/2.2...2.3
[2.2]: https://github.com/troglobit/finit/compare/2.1...2.2
[2.1]: https://github.com/troglobit/finit/compare/2.0...2.1
[2.0]: https://github.com/troglobit/finit/compare/1.12...2.0
[1.12]: https://github.com/troglobit/finit/compare/1.11...1.12
[1.11]: https://github.com/troglobit/finit/compare/1.10...1.11
[1.10]: https://github.com/troglobit/finit/compare/1.9...1.10
[1.9]: https://github.com/troglobit/finit/compare/1.8...1.9
[1.8]: https://github.com/troglobit/finit/compare/1.7...1.8
[1.7]: https://github.com/troglobit/finit/compare/1.6...1.7
[1.6]: https://github.com/troglobit/finit/compare/1.5...1.6
[1.5]: https://github.com/troglobit/finit/compare/1.4...1.5
[1.4]: https://github.com/troglobit/finit/compare/1.3...1.4
[1.3]: https://github.com/troglobit/finit/compare/1.2...1.3
[1.2]: https://github.com/troglobit/finit/compare/1.1...1.2
[1.1]: https://github.com/troglobit/finit/compare/1.0...1.1
[1.0]: https://github.com/troglobit/finit/compare/0.9...1.0
[0.9]: https://github.com/troglobit/finit/compare/0.8...0.9
[0.8]: https://github.com/troglobit/finit/compare/0.7...0.8
[0.7]: https://github.com/troglobit/finit/compare/0.6...0.7
[0.6]: https://github.com/troglobit/finit/compare/0.5...0.6
[0.5]: https://github.com/troglobit/finit/compare/0.4...0.5
[0.4]: https://github.com/troglobit/finit/compare/0.3...0.4
[0.3]: https://github.com/troglobit/finit/compare/0.2...0.3
[0.2]: https://github.com/troglobit/finit/compare/0.1...0.2
[libuEv]: https://github.com/troglobit/libuev
[dea3ae8]: https://github.com/troglobit/finit/commit/dea3ae8

<!--
  -- Local Variables:
  -- mode: markdown
  -- End:
  -->
