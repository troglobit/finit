HowTo: Finit on Debian GNU/Linux
================================

HowTo use Finit to boot a Debian GNU/Linux system.  It is assumed that
the user has already installed a compiler, C library header files, and
other tools needed to build a GNU configure based project.  I.e., at
the very least:

    root@debian:~# apt install build-essential

Like the [Alpine HowTo](../alpine/), you need to install [libuEv][] and
[libite][], but since this is Debian — which takes infinite care of its
users ♥ ♥ ♥ we don't need to worry about `pkg-config`, except for having
it installed so it can locate the uEv and lite libraries.

> With Debian everything just works!™

... just make sure to install the following, so `ifup` and other basic
tools pre-systemd work as intended.

    root@debian:~# apt install initscripts console-setup

> [!TIP]
> As of Debian 11 (Bullseye), libuev and libite are part of the main
> section of Debian.  So just install the -dev packages :)

The following script can then be used to configure, build, install and
set up your system to run Finit:

    user@debian:~/finit$ contrib/debian/build.sh

However, since `/sbin/init` already exists on your system, the script
creates another entry in your GRUB config by updating `$SUPPORTED_INITS`
in `/etc/grub.d/10_inux` and run `update-grub`.  On reboot you will find
a (finit) entry in the "Advanced options for Debian GNU/Linux" section.
It is of course also possible to change the default init to Finit:

    user@debian:~/finit# cd /sbin
    user@debian:/sbin# sudo mv init oldinit
    user@debian:/sbin# sudo ln -s finit init

Before rebooting, check the default [/etc/finit.conf](finit.conf) and
`/etc/finit.d/*.conf` files.  The build + install script above provides
a few sample `.conf` files.  See `initctl ls` after boot for a list of
enabled and available services, you can then use `enable` and `disable`
comands to `initctl`, followed by `reload` to activate your changes.

You can also use a standard [/etc/rc.local](rc.local) for one-shot tasks
and initialization like keyboard language etc.

> [!NOTE]
> For the X Window system, you may need to `sudo apt install elogind`
> (Bullseye and later), followed by `initctl reload` to activate it (it
> is enabled by default), and logout/login again.  The elogind daemon
> ensures a regular non-root user can start and interact with an X
> session, otherwise keyboard and mouse won't work.  When you're happy,
> you can enable the lightdm.conf, change the default runlevel to 3, and
> presto!  you have a desktop again.


Have fun!  
 /Joachim ツ

[libuEv]: https://github.com/troglobit/libuev
[libite]: https://github.com/troglobit/libite
