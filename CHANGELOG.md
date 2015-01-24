Change Log
==========

All notable changes to the project are documented in this file.


[1.11] - 2015-01-24
-------------------

The [libuEv] release.

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


[1.10] - 2014-11-27
-------------------
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


[1.9] - 2014-04-21
------------------

### Changes
* Add support for an include directive to `.conf` files
* Fallback to `/bin/sh` if user forgets tty setting
* Initial support for restarting lost services during `norespwan`

### Fixes
* Bug fixes, code cleanup
* Handle `SIGHUP` from service callback properly when switching runlevel
* Misc. major (memleak) and minor fixes and additions to `libite/lite.h`


[1.8] - 2013-06-07
------------------

### Changes
* Support for runlevels, with a bootstrap runlevel 'S'
* Support for saving previous and current runlevel to UTMP
* Support for new `finit.conf` commands: run, task, and runlevel
* Support for tty and console commands in `finit.conf`, like services
  but for launching multiple getty logins
* New tty plugin to monitor TTYs coming and going, like USB TTYs

### Fixes
* Bugfixes to libite


[1.7] - 2012-10-08
------------------

### Changes
* Show `__FILE__` in `_d()` debug messages, useful for plugins with
  similarily named callbacks. Also, only in debug mode anyway
* Make sure to cleanup recorded PID when a service is lost.  Required by
  service plugins for their callbacks to work.
* Only clear screen when in verbose mode. Maybe this should be removed
  altogether?

### Fixes
* Bugfix: Do not `free()` static string in `finit.conf` parser


[1.6] - 2012-10-06
------------------

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


[1.5] - 2012-10-03
------------------

### Changes
* Use bootmisc plugin to setup standard FHS 2.3 structure in `/var`
* Added `FLOG_WARN()` syslog macro, for plugins
* Add plugin dependency resolver. Checks `plugin_t` for `.depends`


[1.4] - 2012-10-02
------------------

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


[1.3] - 2012-09-28
------------------

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


[1.2] - 2012-09-27
------------------

### Changes
* Update README with section on building and enviroment variables

### Fixes
* Fix installation paths encoded in finit binary


[1.1] - 2012-09-27
------------------

### Changes
* Rename signal.[ch]-->sig.[ch] to avoid name clash w/ system headers

### Fixes
* Build fixes for ARM eabi/uClibc


[1.0] - 2012-09-26
------------------

### Changes
* New plugin based system for all odd extensions
* New service monitor that restarts services if they die
* New maintainer at GitHub http://github.com/troglobit/finit
* Add standard LICENSE and AUTHORS files
* New focus: embedded systems and small headless servers


[0.6] - 2010-06-14
------------------

* Don't start consolekit manually, dbus starts it (rtp)
* Unmount all filesystems before rebooting
* Disable `USE_VAR_RUN_RESOLVCONF` for Mandriva
* Unset terminal type in Mandriva before running X
* Remove extra sleep in finit-alt before calling services.sh (caio)


[0.5] - 2008-08-21
------------------

* Add option to start dbus and consolekit before the X server
* finit-alt listens to `/dev/initctl` to work with `reboot(8)` (smurfy)
* Write runlevel to utmp, needed by Printerdrake (Pascal Terjan)
* Fix ownership of `/var/run/utmp` (reported by Pascal Terjan)
* Remove obsolete code to load AGP drivers
* Conditional build of `/etc/resolveconf/run` support
* Add support to `/var/run/resolvconf` in Mandriva (blino)


[0.4] - 2008-05-16
------------------

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


[0.3] - 2008-02-23
------------------

* Change poweroff method to `reboot(RB_POWER_OFF)` (Metalshark)
* Remove duplicate `unionctl()` reimplementation error
* Fix string termination in path creation
* Mount `/var/lock` and `/var/run` as tmpfs


[0.2] - 2008-02-19
------------------

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

[libuEv]: https://github.com/troglobit/libuev
[dea3ae8]: https://github.com/troglobit/finit/commit/dea3ae8
[1.11]: https://github.com/troglobit/finit/compare/1.10...HEAD
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

