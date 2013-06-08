==============================================================================
                      Finit | Fast Init Replacement
==============================================================================

Finit is an extremely fast /sbin/init replacement focusing on small and
embedded GNU/Linux systems.  It is the continuation of the original
finit by `Claudio Matsuoka`_ which was reverse engineered from syscalls
of the EeePC fastinit_ daemon -- gaps filled with frog DNA ...

This modern finit has extensions for service monitoring, multiple TTYs,
one-shot tasks, runlevels and plugins for I/O monitoring, service
callbacks and various hooks to extend and adapt it to your needs.

Finit restores the raw performance of your system and gives you full
control of the ``main()`` loop of ``/sbin/init``!


Features
--------

**Service monitoring**
Restarting a service when it exits

**Plugins**
Extend and modify finit behavior. See examples in plugins/ directory.
Plugin capabilities:
   
* Service callbacks — should the service run/restart/stop?
* Task/Run callbacks — a one-shot command, should it run in sequence?
* Hooks — hook into the boot at predefined points to extend finit
* I/O — listen to external events and control finit behaviour/services

Most extensions and functionality not purely related to what an /sbin/init
needs to start a system from the original finit_ has been reactored into
plugins with either hooks or I/O demands.


/etc/finit.conf
---------------

Contrary to most script based init alternatives (`SysV init`_, upstart_,
OpenRC_ and the likes) finit instead reads its configuration from
``/etc/finit.conf``.  Below is a brief list, see the source code for the
full list:

check
    Run fsck on a file system before mounting it

module
    Load a kernel module, with optional arguments

network
    Script or program to bringup networking, with optional arguments

runlevel N
    N is the runlevel number 1-9, where 6 is reserved for reboot

run [RUN_LVLS] /path/to/cmd ARGS -- Optional description
    One-shot command to run in sequence when entering a runlevel, with
    optional arguments and description.  This command is guaranteed to
    be completed before running the next command.

task [RUN_LVLS] /path/to/cmd ARGS -- Optional description
    One-shot like 'run', but starts in parallel with the next command

service [RUN_LVLS] /path/to/daemon ARGS -- Optional description
    Service, or daemon, to be monitored and automatically restarted if
    it exits prematurely.  Please note that you often need to provide
    a --foreground or --no-background argument to most daemons to
    prevent them from forking off to the background.

runparts
    Call run-parts(8) on a directory other than the default /etc/finit.d

tty [RUN_LVLS] DEV
    Run getty on the listed TTY device, in the given runlevels

console DEV
    Some embedded systems have a dedicated console port. This command
    tells finit to not start getty, but instead print a friendly message
    and wait for the user to activate the console with a keypress before
    starting getty.

When running ``make install`` no default ``/etc/finit.conf`` will be
provided since the system requirements differ too much.  Try out the
Debian 6.0 example ``/usr/share/doc/finit/finit.conf`` configuration
that is capable of service monitoring SSH, sysklogd, gdm and a console
getty!


/etc/finit.d
------------

At the end of the boot, when networking and all services are up, finit
calls its built-in run-parts(8) on the ``/etc/finit.d/`` directory, if
it exists.  Similar to how the ``/ec/rc.local`` file works in most other
inits, only finit runs a directory of scripts.  This replaces the
earlier support for a ``/usr/sbin/services.sh`` script in the original
finit_.


Runlevels
---------

Support for runlevels_ is included in Finit from v1.8.  By default all
services, tasks, run commands and TTYs listed without a set of runlevels
get a default set [234] assigned.  The default runlevel after boot is 2.

To specify an allowed set of runlevels for a service, run command, task,
or tty, add [NNN] to it in finit.conf, like this::

  run     [S]      /etc/init.d/acpid start -- Starting ACPI Daemon
  task    [S]      /etc/init.d/kbd start -- Preparing console
  service [S12345] /sbin/klogd -n -x -- Kernel log daemon
  tty     [12345]  /dev/tty1
  tty     [2]      /dev/tty2
  tty     [2]      /dev/tty3
  tty     [2]      /dev/tty4
  tty     [2]      /dev/tty5
  tty     [2]      /dev/tty6

In this example acpid is started once at bootstrap using a conventional
SysV init script. Here the run command was used, meaning the following
task command is not run until the init script has fully completed.

Tasks and services are started in parallell, while run commands are run
in the order listed and subsequent commands are not started until a run
command has completed.

Existing finit.conf files that lack runlevel setting will get a default
runlevel assigned, [234].

Switching between runlevels can be done by calling init with a single
argument, e.g., 'init 5' switches to runlevel 5.


Rebooting and Halting
---------------------

Finit handles SIGUSR1 and SIGUSR2 for reboot and halt, and listens to
``/dev/initctl`` so standard Linux reboot and halt commands should also
work.


Building
--------

The finit build system does not employ the GNU Configure and Build System,
instead standard makefiles are used. The user is encouraged to make source
code changes, using defines and conditionally building plugins instead to
alter the behavior of finit.

The following environment variables are checked by the makefiles and control
what is built and where resulting binaries are installed.

**ROOTDIR=**
   Top directory for building complete system, used in pretty printing

**VERSION=**
   Defaults to the currently released version of finit, e.g., 1.3 but can
   be overridden by packages to add a suffix or completely alter the version.

**CFLAGS=**
   Standard CFLAGS are inherited from the build enviornmant.

**CPPFLAGS=**
   Standard CPPFLAGS are inherited from the build enviornmant.

**LDFLAGS=**
   Standard LDFLAGS are inherited from the build enviornmant.

**LDLIBS=**
   Standard LIBLIBS are inherited from the build enviornmant.

**prefix=**
   Base prefix path for all files, except ``sbinbdir`` and ``sysconfdir``.
   Used in concert with the ``DESTDIR`` variable. Defaults to ``/usr``

**sbindir=**
   Path to where resulting binaries should install to. Used in concert
   with the ``DESTDIR`` variable. Defaults to ``/sbin``

**sysconfdir=**
   Path to where finit configuration files should install to. Used in
   concert with the ``DESTDIR`` variable.  Defaults to ``/etc``, but is
   currently unused.

**PLUGINS=**
   List of stock finit plugins to build and install.

**plugindir=**
   Absolute path to where finit should look for dynamically loadable plugins
   at runtime. At installation prepended by ``DESTDIR`` and ``prefix``.
   Defaults to ``/lib/finit/plugins``

**DESTDIR=**
   Used by packagers and distributions when building a relocatable
   bundle of files. Alwawys prepended to the ``prefix`` destination
   directory.

**Example**::

  $ tar xfJ finit-1.3.tar.xz
  $ PLUGINS="initctl.so hwclock.so" DESTDIR=/tmp/finit/dst \
    make -C finit-1.3/ clean install
  make: Entering directory `/home/troglobit/finit-1.3'
    CC      finit.o
    CC      conf.o
    CC      helpers.o
    CC      sig.o
    CC      svc.o
    CC      plugin.o
    CC      strlcpy.o
    LINK    finit
    CC      plugins/initctl.o
    PLUGIN  plugins/initctl.so
    CC      plugins/hwclock.o
    PLUGIN  plugins/hwclock.so
    INSTALL /tmp/finit/dst/sbin/finit
    INSTALL /tmp/finit/dst/lib/finit/plugins/initctl.so
    INSTALL /tmp/finit/dst/lib/finit/plugins/hwclock.so
  make: Leaving directory `/home/troglobit/finit-1.3'

In this example the `finit-1.3.tar.xz`_ archive is unpacked to the
user's home directory, built and installed to a temporary staging
directory.  The enviroment variables ``DESTDIR`` and ``PLUGINS`` are
changed to suit this particular build.


Running
-------

The default install does not setup finit as the system default
``/sbin/init``, neither does it setup an initial ``/etc/finit.conf``.

It is assumed that users of finit are competent enough to either setup
finit as their default ``/sbin/init`` or alter their respective Grub,
LOADLIN, LILO, U-Boot/Barebox or RedBoot boot loader configuration to
give the kernel the following extra command line::

  init=/sbin/finit


Debugging
---------

Add ``finit_debug``, or ``--debug``, to the kernel command line to
enable trace messages.  A console getty is always started, see the file
``finit.h`` for more useful comile-time tweaks::

  init=/sbin/finit --debug


Contact
-------

Finit is maintained collaborativly at http://github.com/troglobit/finit —
please file a bug report, clone it, or send pull requests for bug fixes and
proposed extensions, or become a co-maintainer by contacting the main author.

Regards
 /Joachim Nilsson <troglobit@gmail.com>

.. _`Claudio Matsuoka`: http://helllabs.org/finit/
.. _fastinit: http://wiki.eeeuser.com/boot_process:the_boot_process
.. _`SysV init`: http://savannah.nongnu.org/projects/sysvinit
.. _upstart: http://upstart.ubuntu.com/
.. _runlevels: http://en.wikipedia.org/wiki/Runlevel
.. _openrc: http://www.gentoo.org/proj/en/base/openrc/
.. _`finit-1.3.tar.xz`: ftp://troglobit.com/finit/finit-1.3.tar.xz
