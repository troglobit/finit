==============================================================================
        Finit — A fast plugin-based /sbin/init with service monitoring
==============================================================================

Welcome to finit!

This is an extremely fast /sbin/init replacement with focus on small
embedded GNU/Linux systems.  Based on Claudio Matsuoka's original finit_
with I/O, service and hook plugin extensions.  The original was a
reimplementation of the EeePC fastinit_ daemon based on its system
calls with gaps filled with frog DNA.

In the world of finit we write C plugins that hook into and extend the
``main()`` loop of ``/sbin/init``, this is what gives the system its raw
performance and the user full control.

If you chose finit you know what you want, a fast no hassles boot!


Features
--------

**Service monitoring**
   Restarting a service when it exits

**Plugins**
   Extend and modify finit behavior. See examples in plugins/ directory.
   Plugin capabilities:
   
   * Service callbacks — should the service run/restart/stop?
   * Hooks — hook into the boot at predefined points to extend finit
   * I/O — listen to external events and control finit behaviour/services

Most extensions and functionality not purely related to what an /sbin/init
needs to start a system from the original finit_ has been reactored into
plugins with either hooks or I/O demands.


/etc/finit.conf
---------------

Contrary to most script based init alternatives (`SysV init`_, upstart_,
OpenRC_ and the likes) finit instead reads its configuration from
``/etc/finit.conf``, see the source code for available options.

When running ``make install`` no default ``/etc/finit.conf`` will be
provided since the system requirements differ too much.  Try out the
Debian 6.0 example ``/usr/share/doc/finit/finit.conf`` configuration
that is capable of service monitoring SSH, sysklogd, gdm and a console
getty!


/etc/finit.d
------------

At the end of the boot, when networking and all services are up, finit
calls its built-in run-parts(8) on the ``/etc/finit.d/`` directory, akin
to how the ``/ec/rc.local`` file works in most other inits, only finit
runs a directory of scripts.  This replaces the earlier
``/usr/sbin/services.sh`` support of the original finit_.


Runlevels
---------

Very simple support for runlevels_ exist.  Currently the system will
always start in runlevel 3 and all services belonging to that runlevel
will be started at boot.

To specify an allowed set of runlevels for a service, add [NNN] to its
service stanza in finit.conf, like this:

        service [345] /sbin/klogd -n -x -- Kernel log daemon

Meaning that klogd is allowed to run in runlevels 3, 4 and 5, only.

        service [!06] /sbin/klogd -n -x -- Kernel log daemon

Meaning all runlevels *but* 0 and 6.

Existing finit.conf files that lack runlevel setting will get a default
runlevel assigned, [345].

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

.. _finit: http://helllabs.org/finit/
.. _fastinit: http://wiki.eeeuser.com/boot_process:the_boot_process
.. _`SysV init`: http://savannah.nongnu.org/projects/sysvinit
.. _upstart: http://upstart.ubuntu.com/
.. _runlevels: http://en.wikipedia.org/wiki/Runlevel
.. _openrc: http://www.gentoo.org/proj/en/base/openrc/
.. _`finit-1.3.tar.xz`: http://github.com/downloads/troglobit/finit/finit-1.3.tar.xz
