==============================================================================
                                    NEWS
==============================================================================

1.9 (2014-04-21)
----------------
* Bug fixes, code cleanup
* Add support for an include directive to .conf files
* Fallback to ``/bin/sh`` if user forgets tty setting
* Handle ``SIGHUP`` from service callback properly when switching runlevel
* Initial support for restarting lost services during ``norespwan``
* Misc. major (memleak) and minor fixes and additions to libite/lite.h


1.8 (2013-06-07)
----------------
* Support for runlevels, with a bootstrap runlevel 'S'
* Support for saving previous and current runlevel to UTMP
* Support for new finit.conf commands: run, task, and runlevel
* Support for tty and console commands in finit.conf, like services but
  for launching multiple getty logins
* New tty plugin to monitor TTYs coming and going, like USB TTYs
* Bugfixes to libite


1.7 (2012-10-08)
----------------
* Show __FILE__ in _d() debug messages, useful for plugins with
  similarily named callbacks. Also, only in debug mode anyway
* Make sure to cleanup recorded PID when a service is lost.  Required by
  service plugins for their callbacks to work.
* Bugfix: Do not free() static string in finit.conf parser
* Only clear screen when in verbose mode. Maybe this should be removed
  altogether?


1.6 (2012-10-06)
----------------
* Skip . and .. in plugin loader and display error on failed plugins
* Support for overriding /etc/finit.d with "runparts DIR" in finit.conf
* Revoke support for starting services not starting with a slash.
* Prevent endless restart of non-existing services in finit.conf
* Support for sysvinit style startstop scripts in /etc/finit.d
* Minor fix to alsa-utils plugin to silence on non-existing cards


1.5 (2012-10-03)
----------------
* Use bootmisc plugin to setup standard FHS 2.3 structure in /var
* Added FLOG_WARN() syslog macro, for plugins
* Add plugin dependency resolver. Checks plugin_t for .depends


1.4 (2012-10-02)
----------------
* Fix I/O plugin watcher and load plugins earlier for a new hook
* Start refactoring helpers.c into a libite.so (-lite).  This means
  other user space applications/daemons can make use of the neat toolbox
  available in finit
* Use short-form -s/-w -u to work with BusyBox hwclock as well
* Use RTLD_GLOBAL flag to tell dynamic loader to load dependent .so
  files as well.  Lets other plugins use global symbols.
* Greatly simplify svc hook for external plugins and cleanup plugin API.
* And more... see the GIT log for more details.


1.3 (2012-09-28)
----------------
* Cleanup public plugin API a bit and add new pid/pidfile funcs
* Add plugin hook to end of service startup
* Remove finit.h from svc.h, plugins should not need this.
* Move utility macros etc. to helpers.h
* Make finit.h daemon internal, only
* Move defines of FIFO, conf and rcS.d to Makefile => correct paths
* Add support for installing required headers in system include dir
* Better support for distributions and packagers with install-exec,
  install-data, and install-dev targets in Makefile.  Useful if you want
  to call targets with different $DESTDIR!
* Makefile fixes for installation, paths encoded wrong
* Strip binaries + .so files, support for $(CROSS) toolchain strip
* Default install is now to /sbin/finit and /usr/
* Note change in $PLUGIN_DIR environemnt variable to $plugindir


1.2 (2012-09-27)
----------------
* Fix installation paths encoded in finit binary
* Update README with section on building and enviroment variables


1.1 (2012-09-27)
----------------
* Build fixes for ARM eabi/uClibc
* Rename signal.[ch]-->sig.[ch] to avoid name clash w/ system headers


1.0 (2012-09-26)
----------------
* New plugin based system for all odd extensions
* New service monitor that restarts services if they die
* New maintainer at GitHub http://github.com/troglobit/finit
* Add standard LICENSE and AUTHORS files
* New focus: embedded systems and small headless servers


0.6 (2010-06-14)
----------------
* don't start consolekit manually, dbus starts it (rtp)
* unmount all filesystems before rebooting
* disable USE_VAR_RUN_RESOLVCONF for Mandriva
* unset terminal type in Mandriva before running X
* remove extra sleep in finit-alt before calling services.sh (caio)


0.5 (2008-08-21)
----------------
* add option to start dbus and consolekit before the X server
* finit-alt listens to /dev/initctl to work with reboot(8) (smurfy)
* write runlevel to utmp, needed by Printerdrake (Pascal Terjan)
* fix ownership of /var/run/utmp (reported by Pascal Terjan)
* remove obsolete code to load AGP drivers
* conditional build of /etc/resolveconf/run support
* add support to /var/run/resolvconf in Mandriva (blino)


O.4 (2008-05-16)
----------------
* default username for finit-alt configurable in Makefile
* create loopback device node in finit-alt (for squashfs)
* add option to use built-in run-parts instead of /bin/run-parts
* ignore signal instead of setting to an empty handler (Metalshark)
* handle pam_console permissions in finit-alt for Mandriva
* add services.sh example and nash-hotplug patch for Mandriva
* mount /proc/bus/usb in Mandriva
* add runtime debug to finit-alt if finit_debug parameter is specified
* read configuration from /etc/finit.conf
* run getty with openvt on the virtual terminal


0.3 (2008-02-23)
----------------
* Change poweroff method to reboot(RB_POWER_OFF) (Metalshark)
* Remove duplicate unionctl() reimplementation error
* Fix string termination in path creation
* Mount /var/lock and /var/run as tmpfs


0.2 (2008-02-19)
----------------
* replace system("touch") with touch() in finit-mod (Metalshark)
* disable NO_HCTOSYS by default to match stock Eeepc kernel
* drop system("rm -f") to clean /tmp, its a fresh mounted tmpfs
* write ACPI sleep state to /sys/power/state instead of /proc/acpi/sleep
  (Metalshark)
* use direct calls to set loopback instead of system("ifconfig")
* replace system("cat") and system("dd") with C implementation
* moved finit-mod and finit-alt helpers to helpers.c
* replace system("echo;cat") to draw shutdown splash with C calls


0.1 (2008-02-14)
----------------
* initial release


.. Local Variables:
..  mode: rst
..  version-control: t
.. End:
