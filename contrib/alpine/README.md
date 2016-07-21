HowTo: Finit on Alpine Linux 3.4
=================================

This is a mini-howto for booting a very basic Alpine Linux with Finit.
It is assumed that the user has already installed a compiler, C library
header files, and other tools needed to build a GNU configure based
project.

To start with you need to first install [libuEv][] and [libite][], by
default they install to `/usr/local` and Alpine's `pkg-config` does not
look for libraries and header files there.  So the `PKG_CONFIG_LIBDIR`
environment variable has to be used:

    alpine:~/finit# PKG_CONFIG_LIBDIR=/usr/local/lib/pkgconfig ./configure \
        --enable-embedded --enable-rw-rootfs --enable-inetd-echo-plugin    \
        --enable-inetd-chargen-plugin --enable-inetd-daytime-plugin        \
        --enable-inetd-discard-plugin --enable-inetd-time-plugin           \
        --with-heading="Alpine Linux 3.4" --with-hostname=alpine

The plugins are optional, but the other configure flags are *not*.

    alpine:~/finit# make all install

The installation will skip setting up `/sbin/init`, because it already
exists, and Alpine is hard coded to use it.  So you have to change the
symlink yourself to point to `finit` instead of `/bin/busybox`.

Before rebooting, make sure to install `/etc/finit.conf` and the glue
`/etc/rc.local` from this directory -- check them both if they need to
be tweaked for your installation, you may not run Dropbear SSH or speak
Swedish ...

[libuEv]: https://github.com/troglobit/libuev
[libite]: https://github.com/troglobit/libite

<!--
  -- Local Variables:
  -- mode: markdown
  -- End:
  -->
