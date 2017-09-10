HowTo: Finit on Alpine Linux 3.4-3.6
====================================

HowTo use Finit to boot an Alpine Linux system.  It is assumed that the
user has already installed make, a compiler, C library header files, and
other tools needed to build a GNU configure based project.

To start with you need to first install [libuEv][] and [libite][].  They
default to install to `/usr/local`, but unlike Debian and Ubuntu based
distros, Alpine's `pkg-config` does not look for libraries and header
files there.  So the `PKG_CONFIG_LIBDIR` environment variable must be
used, or change the install prefix to `/usr`.

The bundled `build.sh` script can be used to configure and build finit:

    alpine:~/finit# ./contrib/alpine/build.sh
    alpine:~/finit# make install

The installation skips `/sbin/init`, because it already exists, and
Alpine is hard coded to use it.  So you have to change the symlink
yourself to point to `finit` instead of `/bin/busybox`.

    alpine:~/finit# cd /sbin
    alpine:/sbin# rm init
    alpine:/sbin# ln -s finit init

Before rebooting, make sure to set up a [/etc/finit.conf](finit.conf),
and [/etc/finit.d/](finit.d) for your services.  Samples are included in
this directory.  Notice the symlinks in `/etc/finit.d/`, you can create
them yourself or add them at runtime using `initctl enable SERVICE`.
You can also use a standard [/etc/rc.local](rc.local) for one-shot set
up and initialization like keyboard language etc.

[libuEv]: https://github.com/troglobit/libuev
[libite]: https://github.com/troglobit/libite

<!--
  -- Local Variables:
  -- mode: markdown
  -- End:
  -->
