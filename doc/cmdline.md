Tips & Tricks with the kernel cmdline
=====================================

This document summarizes the different boot parameters that can be
passed on the Linux kernel command line.  Not limited to Finit.

* `debug`__
    Enable kernel debug.  Debug messages are printed to the console
    until Finit starts up, the syslog daemon then empties `/dev/kmsg`

* `init=/bin/sh`  
    Bypass system default init and tell kernel to start a shell.  Note,
	this shell is very limited and does not support signals and has no
	job control.  Recommend using, and modifying, `rescue` mode instead.

* `panic=SEC`  
    By default the kernel does not reboot after a kernel panic.  This
    setting will cause a kernel reboot after SEC seconds.

* `quiet`  
    Suppress kernel logging to console, except for warnings and errors.

* `rescue`  
    Start Finit rescue mode; no network, no `fsck` of file systems in
    `/etc/fstab`, no `/etc/rc.local`, no `runparts`.  The configuration
    is read from `/etc/rescue.conf`, can be modified by the operator.

* `single`, or `S`  
    Single user mode, runlevel 1, in Finit.  Useful to debug problems with
    the regular runlevel.  All services and TTYs in `[1]` will be started,
    so a `tty [1] @console nologin` configuration presents you with a
    root console without login.

For more on kernel boot paramaters, see the man page `bootparam(7)`.
