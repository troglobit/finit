HowTo: Finit on Alpine Linux 3.4
================================

Mini HowTo using Finit to boot an Alpine Linux system.  It is assumed
that the user has already installed a compiler, C library header files,
and other tools needed to build a GNU configure based project.

To start with you need to first install [libuEv][] and [libite][], by
default they install to `/usr/local` and Alpine's `pkg-config` does not
look for libraries and header files there.  So the `PKG_CONFIG_LIBDIR`
environment variable has to be used:

    alpine:~/finit# PKG_CONFIG_LIBDIR=/usr/local/lib/pkgconfig ./configure \
        --enable-rw-rootfs            --enable-x11-common-plugin           \
        --enable-alsa-utils-plugin    --enable-inetd-echo-plugin           \
        --enable-inetd-chargen-plugin --enable-inetd-daytime-plugin        \
        --enable-inetd-discard-plugin --enable-inetd-time-plugin           \
        --with-heading="Alpine Linux 3.4" --with-hostname=alpine

The plugins are optional, but the other configure flags are *not*.

    alpine:~/finit# make all install

The installation skips `/sbin/init`, because it already exists, and
Alpine is hard coded to use it.  So you have to change the symlink
yourself to point to `finit` instead of `/bin/busybox`.

    alpine:~/finit# cd /sbin
    alpine:/sbin# rm init
    alpine:/sbin# ln -s finit init

Before rebooting, make sure to set up a [/etc/finit.conf](finit.conf),
and possibly also an [/etc/rc.local](rc.local) for one-shot set up and
initialization like keyboard language etc.  You can use the files in
this directory as templates -- check them both if they need to be
tweaked for your installation, you may not run Dropbear SSH or speak
Swedish ...

[libuEv]: https://github.com/troglobit/libuev
[libite]: https://github.com/troglobit/libite

<!--
  -- Local Variables:
  -- mode: markdown
  -- End:
  -->
