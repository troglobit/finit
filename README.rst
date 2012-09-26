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

:Service monitoring:
   Restarting a service when it exits

:Plugins:
   Extend and modify finit behavior. See examples in plugins/ directory
   
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

Try out the examples/finit.conf for a Debian 6.0 example configuration
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

