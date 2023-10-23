Hooks & Plugins
===============

* [Plugins](#plugins)
* [Hooks](#hooks)
  * [Bootstrap Hooks](#bootstrap-hooks)
  * [Runtime Hooks](#runtime-hooks)
  * [Shutdown Hooks](#shutdown-hooks)

Finit can be extended to add general functionality in the form of I/O
monitors, or hook plugins.  It is even possible to run scripts at these
hook points, see below.

The following sections detail existing plugins and hook points.  For
more information, see the plugins listed below.


Plugins
-------

For your convenience a set of *optional* plugins are available:

* *alsa-utils.so*: Restore and save ALSA sound settings on
  startup/shutdown.  _Optional plugin._

* *bootmisc.so*: Setup necessary files and system directories for, e.g.,
  UTMP (tracks logins at boot).  This plugin is central to get a working
  system and runs at `HOOK_BASEFS_UP`.  The `/var`, `/run`, and `/dev`
  file systems must be writable for this plugin to work.

  This plugin is a wrapper for the [tmpfiles.d(5)][] implementation that
  Finit has.  Capable but limited: no aging, attributes, or subvolumes.

  By default, `/lib/finit/tmpfiles.d` carries all the default .conf
  files distributed with Finit.  It is read first but can be overridden
  by any of the standard tmpfiles.d directories, e.g. `/etc/tmpfiles.d`.

  > **Note:** On an embedded system both `/var` and `/run` can be `tmpfs`
  > RAM disks and `/dev` is usually a `devtmpfs`.  This must be defined
  > in the `/etc/fstab` file and in the Linux kernel config.

* *dbus.so*: Setup and start system message bus, D-Bus, at boot.
  _Optional plugin._

* *hook-scripts.so*: Trigger the execution of scripts from plugin hook
  points (see [Hooks](#hooks)).  _Optional plugin._

  This plugin is particularly useful for early boot debugging that needs
  to take place before regular services are available.

  For example, say that you want to enable some kernel tracing before
  modules are loaded. With hook scripts, you can just drop in a shell
  script in `/libexec/finit/hook/mount/all/` that will poke the right
  control files in tracefs.

  Scripts are located in `/libexec/finit/hook` by default, this can be
  customized at build-time using the `--with-hook-scripts-path=PATH`
  argument to `configure`.

* *hotplug.so*: Replaced with `/lib/finit/system/10-hotplug.conf`, which
  checks for `udevd` and `mdev` at boot.  This file can be overridden by a
  file in `/etc/finit.d/10-hotplug.conf`.

  Enabled by default.
  
  > See the [Services](config.md#services) section in the configuration
  > guide for an example how to run `mdevd`, alternative to plain mdev.

* *rtc.so*: Restore and save system clock from/to RTC on boot/halt.
  Enabled by default.

* *modules-load.so*: Scans `/etc/modules-load.d/*.conf` for modules to
  load using `modprobe`.  Each file can contain multiple lines with the
  name of the module to load.  Any line starting with the standard UNIX
  comment character, `#`, or `;`, is skipped.
  
  Modules are by default loaded in runlevel `S` using the `task` stanza.
  Each module gets a unique `name:modprobe.foo`, and optional`:ID`.  The
  runlevel can be changed per file using:

        set runlevel 2345

  The `:ID` is a globally incremented index, which can be disabled per
  file (anywhere) using the following config line:

        set noindex

  To change the index used by the plugin:

        set index 1234

  **Note:** unlike the traditional .conf `module` directive, which load
  any listed module immediately, this plugin creates a background `task`
  which load the module(s) in the background.  The program is modprobe,
  `/sbin/modprobe`, which you can override per .conf file:

        set modprobe /path/to/maybe-a-modprobe-wrapper

  Since these tasks run in the background, they usually return `[ OK ]`
  at boot, unless the modprobe tool does not exist.  Check syslog for
  warnings and the actual status of the operation using `initctl`.

* *netlink.so*: Listens to Linux kernel Netlink events for gateway and
  interfaces.  These events are then sent to the Finit service monitor
  for services that may want to be SIGHUP'ed on new default route or
  interfaces going up/down.  Enabled by default.

* *resolvconf.so*: Setup necessary files for `resolvconf` at startup.
  _Optional plugin._

* *tty.so*: Watches `/dev`, using inotify, for new device nodes (TTY's)
  to start/stop getty consoles on them on demand.  Useful when plugging
  in a usb2serial converter to login to your embedded device.  Enabled
  by default.

* *urandom.so*: Setup random seed at startup.  Enabled by default.

* *x11-common.so*: Setup necessary files for X-Window.  _Optional plugin._

Usually you want to hook into the boot process once, simple hook plugins
like `bootmisc.so` are great for that purpose.  They are called at each
hook point in the boot process, useful to insert some pre-bootstrap
mechanisms, like generating configuration files, restoring HW device
state, etc.  Available hook points are:


Hooks
-----

In the below listings, the first label is the hook point for a C plugin,
the second is the condition name and hook script path.  A hook script is
a plain shell script, or program, that does a very small dedicated job
at the below hook points.

See [Run-parts Scripts](config.md#run-parts-scripts) for details on the
requirements, possibilities, and *limitations* of hook scripts.

All hook scripts are called with at least one environment variable set,
the hook name, useful when reusing the same hook script for multiple
hook points:

  - `FINIT_HOOK_NAME`: set to the second label, e.g., `hook/net/up`
  - `FINIT_SHUTDOWN`: set for `hook/sys/shutdown` and later to one
    of `halt`, `poweroff`, or `reboot`.

**Example:**

    $ mkdir -p /libexec/finit/hook/sys/down
    $ cat <<EOF >/libexec/finit/hook/sys/down/foo.sh
    #!/bin/sh
    echo 'I run just before the reboot() syscall at shutdown/reboot'
    echo 'I have access to /dev since devtmpfs is exempt from umount'
    echo "FINIT_HOOK_NAME: $FINIT_HOOK_NAME"
    if [ -n "$FINIT_SHUTDOWN" ]; then
            echo "FINIT_SHUTDOWN:  $FINIT_SHUTDOWN"
    fi
    exit 0
    EOF
    $ chmod +x /libexec/finit/hook/sys/down/foo.sh

> **Note:** to use hook scripts, even for pre-bootstrap and pre-shutdown
> tasks, you must build with `configure --enable-hook-scripts-plugin`.

### Bootstrap Hooks

* `HOOK_BANNER`, `hook/sys/banner`: The very first point at which a
  plugin can run.  Intended to be used as a banner replacement.
  Essentially this runs just before entering runlevel S.  Assume nothing
  is available, so be prepared to use absolute paths, etc.

* `HOOK_ROOTFS_UP`, `hook/mount/root`: When `finit.conf` has been read
  and `/` has is mounted — very early

* `HOOK_MOUNT_ERROR`, `hook/mount/error`: executed if `mount -a` fails

* `HOOK_MOUNT_POST`, `hook/mount/post`: always executed after `mount -a`

* `HOOK_BASEFS_UP`, `hook/mount/all`: All of `/etc/fstab` is mounted,
  swap is available and default init signals are setup

* `HOOK_SVC_PLUGIN`, `hook/svc/plugin`: Called in `conf_init()` right
  before loading `/etc/finit.conf`.  For plugins to register any early
  run/task/services, please do NOT use any earlier hook point.  That
  will cause uninitialized rlimits that lead to unpredictable results
  when Finit later tries to start the run/task/service.

* `HOOK_NETWORK_UP`, `hook/net/up`: System bootstrap, runlevel S, has
  completed and networking is up (`lo` is up and the `network` script
  has run)

* `HOOK_SVC_UP`, `hook/svc/up`: All services in the active runlevel have
  been launched

* `HOOK_SYSTEM_UP`, `hook/sys/up`: All services *and* everything in
  `/etc/finit.d` has been launched

### Runtime Hooks

* `HOOK_SVC_RECONF`, N/A: Called when the user has changed something in
  the `/etc/finit.d` directory and issued `SIGHUP`.  The hook is called
  when all modified/removed services have been stopped.  When the hook
  has completed, Finit continues to start all modified and new services.

* `HOOK_RUNLEVEL_CHANGE`, N/A: Called when the user has issued a
  runlevel change.  The hook is called when services not matching the
  new runlevel have been been stopped.  When the hook has completed,
  Finit continues to start all services in the new runlevel.

### Shutdown Hooks

* `HOOK_NETWORK_DN`, `hook/net/down`: Called right after having changed
  to runlevel 6, or 0, when all services have received their 'stop' signal.

* `HOOK_SHUTDOWN`, `hook/sys/shutdown`: Called at shutdown/reboot, right
  before all services are sent `SIGTERM`

* `HOOK_SVC_DN`, `hook/svc/down`: In shutdown/reboot, all services and
  non-reserved processes have been killed.  **Note:** only hook scripts
  can run here!

* `HOOK_SYSTEM_DN`, `hook/sys/down`: In shutdown/reboot, called after
  all non-reserved file systems have been unmounted, just before Finit
  tells the kernel to reboot or shut down.  **Note:** only hook scripts
  can run here!

Plugins like `tty.so` extend finit by acting on events, they are called
I/O plugins and are called from the finit main loop when `poll()`
detects an event.  See the source code for `plugins/*.c` for more help
and ideas.

[tmpfiles.d(5)]: https://www.freedesktop.org/software/systemd/man/tmpfiles.d.html
