Building
========

Finit comes with a traditional configure script to control features and
optional plugins to enable.  It does however depend on two external
libraries that provide some frog DNA needed:

- [libuEv][]
- [libite][] (-lite)

and has the following runtime depends:

- grep
- cat
- tail

Like most free/open source software that uses `configure` they deafult
to install to `/usr/local`.  However, some Linux distributions do no
longer search that path for installed software, e.g. Fedora and Alpine
Linux.  To get finit's configure script to find its dependencies you
have to help the `pkg-config` tool a bit if you do not change the
default prefix path:

    PKG_CONFIG_LIBDIR=/usr/local/lib/pkgconfig ./configure

Below are a few of the main switches to
configure:

* `--disable-inetd`: Disable the built-in inetd server.

* `--enable-rw-rootfs`: Most desktop and server systems boot with the
  root file stystem read-only.  With this setting Finit will remount it
  as read-write early at boot so the `bootmisc.so` plugin can run.
  Usually not needed on embedded systems.

* `--enable-static`: Build Finit statically.  The plugins will be
  built-ins (.o files) and all external libraries, except the C library
  will be linked statically.

* `--enable-alsa-utils`: Enable the optional `alsa-utils.so` sound plugin.

* `--enable-dbus`: Enable the optional D-Bus `dbus.so` plugin.

* `--enable-lost`: Enable noisy example plugin for `HOOK_SVC_LOST`.

* `--enable-resolvconf`: Enable the `resolvconf.so` optional plugin.

* `--enable-x11-common`: Enable the optional X Window `x11-common.so` plugin.

For more configure flags, see <kbd>./configure --help</kbd>

**Example**

First, unpack the archive:

```shell
    $ tar xf finit-3.0.tar.xz
    $ cd finit-3.0/
```

Then configure, build and install:

```shell
    $ ./configure --enable-rw-rootfs            --enable-inetd-echo-plugin        \
                  --enable-inetd-chargen-plugin --enable-inetd-daytime-plugin     \
                  --enable-inetd-discard-plugin --enable-inetd-time-plugin        \
                  --with-heading="Alpine Linux 3.4" --with-hostname=alpine
    $ make
    .
    .
    .
    $ DESTDIR=/tmp/finit make install
```

In this example the [finit-3.0.tar.xz][1] archive is unpacked to the
user's home directory, configured, built and installed to a temporary
staging directory.  The environment variable `DESTDIR` controls the
destination directory when installing, very useful for building binary
standalone packages.

Finit 3.0 and later can detect if it runs on an embedded system, or a
system that use BusyBox tools instead of udev & C:o.  On such systems
`mdev` instead of `udev` is used.  However, remember to also change the
Linux config to:

    CONFIG_UEVENT_HELPER_PATH="/sbin/mdev"

**Note:** If you run into problems starting Finit, take a look at
  `finit.c`.  One of the most common problems is a custom Linux kernel
  build that lack `CONFIG_DEVTMPFS`.  Another is too much cruft in the
  system `/etc/fstab`.


[1]:       ftp://troglobit.com/finit/finit-3.0.tar.xz
[libuEv]:  https://github.com/troglobit/libuev
[libite]:  https://github.com/troglobit/libite

<!--
  -- Local Variables:
  -- mode: markdown
  -- End:
  -->
