Hooks & Plugins
===============

* [Plugins](#plugins)
* [Hooks](#hooks)
  * [Bootstrap Hooks](#bootstrap-hooks)
  * [Runtime Hooks](#runtime-hooks)
  * [Shutdown Hooks](#shutdown-hooks)

Finit can be extended to add general functionality in the form of I/O
monitors, built-in inetd services, or hook plugins.

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

  Note: On an embedded system both `/var` and `/run` can be `tmpfs` RAM
  disks and `/dev` is usually a `devtmpfs`.  This must be defined in the
  `/etc/fstab` file and in the Linux kernel config.

* *dbus.so*: Setup and start system message bus, D-Bus, at boot.
  _Optional plugin._

* *echo.so*: RFC 862 plugin.  Start as inetd service, like time below.

* *chargen.so*: RFC 864 plugin.  Start as inetd service, like time below.

* *daytime.so*: RFC 867 plugin.  Start as inetd service, like time below.

* *discard.so*: RFC 863 plugin.  Start as inetd service, like time below.

* *rtc.so*: Restore and save system clock from/to RTC on boot/halt.

* *initctl.so*: Extends finit with a traditional `initctl` functionality.

* *modules-load.so*: Scans /etc/modules-load.d for modules to modprobe.

* *netlink.so*: Listens to Linux kernel Netlink events for gateway and
  interfaces.  These events are then sent to the Finit service monitor
  for services that may want to be SIGHUP'ed on new default route or
  interfaces going up/down.

* *resolvconf.so*: Setup necessary files for `resolvconf` at startup.
  _Optional plugin._

* *time.so*: RFC 868 (rdate) plugin.  Start as inetd service.  Useful
  for testing inetd filtering — BusyBox has an rdate (TCP) client.

* *tty.so*: Watches `/dev`, using inotify, for new device nodes (TTY's)
  to start/stop getty consoles on them on demand.  Useful when plugging
  in a usb2serial converter to login to your embedded device.

* *urandom.so*: Setup random seed at startup.

* *x11-common.so*: Setup necessary files for X-Window.  _Optional plugin._

Usually you want to hook into the boot process once, simple hook plugins
like `bootmisc.so` are great for that purpose.  They are called at each
hook point in the boot process, useful to insert some pre-bootstrap
mechanisms, like generating configuration files, restoring HW device
state, etc.  Available hook points are:


Hooks
-----

### Bootstrap Hooks

* `HOOK_ROOTFS_UP`: When `finit.conf` has been read and `/` has is
  mounted — very early

* `HOOK_BASEFS_UP`: All of `/etc/fstab` is mounted, swap is available
  and default init signals are setup

* `HOOK_NETWORK_UP`: System bootstrap, runlevel S, has completed and
  networking is up (`lo` is up and the `network` script has run)

* `HOOK_SVC_UP`: All services in the active runlevel has been launched

* `HOOK_SYSTEM_UP`: All services *and* everything in `/etc/finit.d`
  has been launched

### Runtime Hooks

* `HOOK_SVC_RECONF`: Called when the user has changed something in the
  `/etc/finit.d` directory and issued `SIGHUP`.  The hook is called when
  all modified/removed services have been stopped.  When the hook has
  completed, Finit continues to start all modified and new services.

* `HOOK_RUNLEVEL_CHANGE`: Called when the user has issued a runlevel
  change.  The hook is called when services not matching the new
  runlevel have been been stopped.  When the hook has completed, Finit
  continues to start all services in the new runlevel.

### Shutdown Hooks

* `HOOK_SHUTDOWN`: Called at shutdown/reboot, right before all
  services are sent `SIGTERM`

Plugins like `initctl.so` and `tty.so` extend finit by acting on events,
they are called I/O plugins and are called from the finit main loop when
`poll()` detects an event.  See the source code for `plugins/*.c` for
more help and ideas.
