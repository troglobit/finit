==============================================================================
        Finit — A fast plugin-based /sbin/init with service monitoring
==============================================================================

Welcome to finit!

This is a heavily extended version of the original /sbin/init replacement
finit_.  The original was a reimplementation of the EeePC "fastinit" daemon
based on its system calls with gaps filled with frog DNA.

Focus of this extended version is on small embedded GNU/Linux systems and
headless miniature servers with a need for extremely fast boot.  In such
environments configurations rarely change and setup is rather static and
straight forward.

In the world of finit we write C "scripts" to boot our system, this is what
gives the system its raw performance and the user full control.

If you chose finit you know what you want, a fast boot with no hassle.


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

Contrary to most script based init alternatives (SysV init_, upstart_, OpenRC_
and the likes) finit reads its configuration from /etc/finit.conf, see the
source code for available options.

When running ''make install'' no default ''/etc/finit.conf'' will be
provided since the system requirements differ too much.  Try out the
Debian 6.0 example /usr/share/doc/finit/finit.conf configuration that is
capable of service monitoring SSH, sysklogd, gdm and a console getty!


/etc/finit.d
------------

At the end of the boot, when networking and all services are up, finit calls
its built-in run-parts(8) on the /etc/finit.d/ directory, akin to how the
/ec/rc.local file works in most other inits, only finit runs a directory of
scripts.  This replaces the earlier /usr/sbin/services.sh support of the
original finit_.


Rebooting and Halting
---------------------

Finit handles SIGUSR1 and SIGUSR2 for reboot and halt, and listens to
/dev/initctl so standard Linux reboot and halt commands should also
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
   Base prefix path for all files, except ''sbinbdir'' and ''sysconfdir''.
   Used in concert with the ''DESTDIR'' variable. Defaults to /usr

**sbindir=**
   Path to where resulting binaries should install to. Used in concert
   with the ''DESTDIR'' variable. Defaults to /sbin

**sysconfdir=**
   Path to where finit configuration files should install to. Used in
   concert with the ''DESTDIR'' variable.  Defaults to /etc, but is
   currently unused.

**PLUGINS=**
   List of stock finit plugins to build and install.

**plugindir=**
   Absolute path to where finit should look for dynamically loadable plugins
   at runtime. At installation prepended by ''DESTDIR'' and ''prefix''.
   Defaults to /lib/finit/plugins

**DESTDIR=**
   Used by packagers and distributions when building a relocatable
   bundle of files. Alwawys prepended to the ''prefix'' destination
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

In this example the finit-1.3.tar.xz archive is unpacked to the user's
home directory, built and installed to a temporary staging directory.
The enviroment variables ''prefix'', ''DESTDIR'' and ''PLUGINS'' are all
changed to suit this particular build.


Debugging
---------

Add finit_debug, or --debug, to the kernel command line to enable trace
messages.  A console getty is always started, see the file finit.h for
more useful comile-time tweaks.

Contact
-------

Finit is maintained collaborativly at https://github.com/troglobit/finit —
please file a bug report, clone it, or send pull requests for bug fixes and
proposed extensions, or become a co-maintainer by contacting the main author.

Regards
 /Joachim Nilsson <troglobit@gmail.com>

.. _finit: http://helllabs.org/finit/
.. _init: http://savannah.nongnu.org/projects/sysvinit
.. _upstart: http://upstart.ubuntu.com/
.. _openrc: http://www.gentoo.org/proj/en/base/openrc/

