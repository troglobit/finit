HowTo: Finit on Void Linux
==========================

HowTo use Finit to boot an [Void Linux][] system.  It is assumed that
the user has already installed make, a compiler, C library header files,
and other tools needed to build a GNU configure based project.

To start with you need to first install [libuEv][] and [libite][].  They
default to install to `/usr/local`, but unlike Debian and Ubuntu based
distros, Void's `pkg-config` does not look for libraries and header
files there.  So the `PKG_CONFIG_LIBDIR` environment variable must be
used, or change the install prefix to `/usr`.

The bundled [build.sh](build.sh) script can be used to configure and
build finit:

    void:~# cd finit
    void:~/finit# ./contrib/void/build.sh

Then run the [install.sh](install.sh) script to install all necessary
files, including the sample `finit.conf` and `finit.d/*.conf` files.
More on that below.

    void:~/finit# ./contrib/void/install.sh

The install script is (supposed to be) non-destructive by default, you
have to answer *Yes* twice to set up Finit as the system default init.
Pay close attention to the last question:

    *** Install Finit as the system default Init (y/N)?

If you answer `No`, simply by pressing enter, you can change the symlink
yourself later on, to point to `finit` instead of `runit`:

    void:~/finit# cd /sbin
    void:/sbin# rm init
    void:/sbin# ln -s finit init

Another option is to change the Grub defaults, in `/etc/default/grub`:

    #GRUB_CMDLINE_LINUX_DEFAULT="loglevel=4 slub_debug=P page_poison=1"
    GRUB_CMDLINE_LINUX_DEFAULT="loglevel=4 init=/sbin/finit"

Alternatively, you can simply modify the default Grub entry at boot, or
set up an alternative Grub entry to include:

    linux /boot/vmlinuz-X.YY.Z ... init=/sbin/finit

If you modify a Grub configuration file, remember to run:

    update-grub

Before rebooting, make sure to set up a [/etc/finit.conf](finit.conf),
and [/etc/finit.d/](finit.d/) for your services.  Samples are included
in this directory.  Notice the symlinks in `/etc/finit.d/`, which can be
managed by the operator at runtime using `initctl enable SERVICE`.  You
can also use a standard [/etc/rc.local](rc.local) for one-shot tasks and
initialization like keyboard language etc.

[libuEv]: https://github.com/troglobit/libuev
[libite]: https://github.com/troglobit/libite
[Void Linux]: https://www.voidlinux.eu/
