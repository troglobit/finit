Tips & Tricks with the kernel cmdline
=====================================

This document summarizes the different boot parameters that can be
passed on the Linux kernel command line.  Not limited to Finit.

The `bool` setting is one of `on, off, true false, 1, 0`.

> **NOTE:** remember to use `--` to separate kernel parameters from
> parameters to init.  E.g., `init=/sbin/finit -- finit.debug rescue`

* `debug`: Enable kernel debug.  Debug messages are printed to the
    console until Finit starts up.

* `finit.debug[=bool]`: Enable finit debug.  This is operated
	independently of the kernel debug setting.  New as of Finit v4.

* `finit.show_status[=bool]`: Control finit boot progress, including
  banner.

* `finit.status_style=<classic,modern>`: Set Finit boot progress style,
  when enabled.

* `init=/bin/sh`: Bypass system default init and tell kernel to start a
	shell.  Note, this shell is very limited and does not support
	signals and has no job control.  Recommend using, and modifying,
	`rescue` mode instead.

* `panic=SEC`: By default the kernel does not reboot after a kernel
    panic.  This setting will cause a kernel reboot after SEC seconds.

* `quiet`: Suppress kernel logging to console, except for warnings and
  errors.

* `rescue`: Start rescue/maintenance mode.  If your system comes with
    the bundled `sulogin` program (Finit, or from util-linux/Busybox),
    you will be given a root login to a maintenance shell.  However, if
    `sulogin` is missing, the file `/lib/finit/rescue.conf` is read and
    the system booted in a limited fallback mode.  See [config.md][]
	for more information.

* `single`, or `S`: Single user mode, runlevel 1, in Finit.  Useful to
    debug problems with the regular runlevel.  All services and TTYs in
    `[1]` will be started, so a `tty [1] @console nologin` configuration
    presents you with a root console without login.

For more on kernel boot parameters, see the man page [bootparam(7)][].

[config.md][]:  config.md#rescue-mode
[bootparam(7)]: https://www.man7.org/linux/man-pages/man7/bootparam.7.html
