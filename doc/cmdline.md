Tips & Tricks with the kernel cmdline
=====================================

This document summarizes the different boot parameters that can be
passed on the Linux kernel command line.  Not limited to Finit.

The `bool` setting is one of `on, off, true false, 1, 0`.

> **NOTE:** remember to use `--` to separate kernel parameters from
> parameters to init.  E.g., `init=/sbin/finit -- finit.debug rescue`

* `debug`: Enable kernel debug.  Debug messages are printed to the
   console until Finit starts up, unless `loglevel=7` (below) is used.

* `fsck.mode=<auto,force,skip>`, default: `auto`, unless built with
  `configure --enable-fastboot`, in which case the default is `skip`

* `fsck.repair=<preen,yes,no>`, default: `preen`, unless built with
  `configure --enable-fsckfix`, in which case the default is `yes`.
  This configure option also sets `fsck.mode=force`, unless fastboot
  is selected at the same time.  I.e., the `--enable-fastboot` build
  option overrides the `fsck.mode` default value.

* `finit.cond=foo[,bar[,baz]]`: set `<boot/foo>` condition, optionally
  multiple conditions can be set using the same option, separated with a
  comma.  Alternatively, multiple `foo.cond=arg` can be given.  Each will
  result in a `<boot/arg>` condition being set to control the rest of the
  system bootstrap.

  Very useful for selecting different boot modes, e.g. manufacturing test,
  firmware upgrade, or rescue mode.

  > Note: `<boot/...>` conditions cannot be cleared with `initctl`!

* `finit.config=/path/to/alternative/finit.conf`: override the
  compile-time bootstrap config file, default:

        ./configure --with-config=/etc/finit.conf

   Useful when starting up in various rescue mode, factory, or
   production test setups.  Use the top-level configuration file
   directive `rcsd /path/to/finit.d` to override the default
   rcS.d directory.

* `finit.debug[=bool]`: Enable finit debug.  This is operated
	independently of the kernel `debug` setting.  New as of Finit v4.

* `finit.fstab=</path/to/etc/fstab.alternative>`: Tell Finit to use an
  alternate `fstab` to mount the file system from.  Remember, this file
  must be on the `root=...` file system provided to Finit from the
  kernel.  By default the built-in fstab is used, which itself defaults
  to `/etc/fstab`, but can be changed at build time with:

        ./configure --with-fstab=/path/to/fstab

  It is even possible to disable a built-in default using:

        ./configure --without-fstab

  Making `finit.fstab=/path/to/fstab` a *mandatory* command line option.
  Note, if the command line fstab is missing, Finit falls back to the
  built-in fstab, and if both are missing, the system treats this as a
  bad `fsck` and thus calls `sulogin`.  If, in turn, `sulogin` is not
  available on the system, Finit calls reboot, which is also what will
  happen when a user exits from `sulogin`.

* `finit.status[=bool]`: Control finit boot progress, including banner.
  (Used to be `finit.show_status`, which works but is deprecated.)

* `finit.status_style=<classic,modern>`: Set Finit boot progress style,
  when enabled.

* `init=/bin/sh`: Bypass system default init and tell kernel to start a
	shell.  Note, this shell is very limited and does not support
	signals and has no job control.  Recommend using, and modifying,
	`rescue` mode instead.

* `loglevel=<0-7>`, sets the kernel's log level, which is more granular
  than `debug`.  Also, when `loglevel=7`, Finit will *not disable*
  kernel logs to the console.  This is very useful when debugging the
  kernel at system bring-up.  Since `loglevel=7` is the same as `debug`
  this means you have to use `quiet` for a quiet boot, until sysklogd
  takes over logging of kernel events.

* `panic=SEC`: By default the kernel does not reboot after a kernel
    panic.  This setting will cause a kernel reboot after SEC seconds.

* `quiet`: Suppress kernel logging to console, except for warnings and
  errors.  Also, see `loglevel` and `quiet` above.

* `rescue`: Start rescue/maintenance mode.  If your system comes with
    the bundled `sulogin` program (Finit, or from util-linux/Busybox),
    you will be given a root login to a maintenance shell.  However, if
    `sulogin` is missing, the file `/lib/finit/rescue.conf` is read and
    the system booted in a limited fallback mode.  See [config.md][]
    for more information.

    This option can be disabled with `configure --without-rescue`

    **Note:** in this mode `initctl` will not work.  Correct the problem
    and use `reboot -f` to force reboot.

* `single`, or `S`: Overrides the configured runlevel (default: 2) to go
    to after bootstrap by forcing it to runlevel 1, this is also known
    as single user mode.  Useful to debug startup problems.  All services
    and TTYs in `[1]` will be started, so a `tty [1] @console nologin`
    configuration presents you with a root console without login.

 * `1-9`, except `6`: override the configured `runlevel`.  Like the `S`
   and `rescue`, giving a single number on the kernel command line tells
   Finit to ignore any `runlevel` in `/etc/finit.conf` as well as the
   configure fallback `--with-runlevel=N` setting.  Remember, `6` is the
   reboot runlevel and is not permitted.  Any other values are ignored.

For more on kernel boot parameters, see the man page [bootparam(7)][].

[config.md]:    config.md#rescue-mode
[bootparam(7)]: https://www.man7.org/linux/man-pages/man7/bootparam.7.html
