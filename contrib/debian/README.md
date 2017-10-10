HowTo: Finit on Debian GNU/Linux
================================

HowTo use Finit to boot a Debian GNU/Linux system.  It is assumed that
the user has already installed a compiler, C library header files, and
other tools needed to build a GNU configure based project.

Also, Finit has only been verified to work with the Debian *Server* ed.
I.e., during install — skip GNOME and other X stuff to get a plain and
usable Debian server install, which we can add X-Window to later :-)

Now, I started off by replacing systemd from the base install with the
old SysV init, using [this HowTo][1], you may not need to do that, but
for me it was easier — and also a great rebuttal to all the systemd
haters out there (yeah, even though I speak for the alternative PID 1
movement I don't hate systemd :P) it is possible to still run Debian
without systemd!  Anyway, this text was based on that experience.

Like the [Alpine HowTo](../alpine/), you need to install [libuEv][] and
[libite][], but since this is Debian — which takes infinite care of its
users ♥ ♥ ♥ we don't need to worry about `pkg-config`, except for having
it installed so it can locate the uEv and lite libraries.

> With Debian everything just works!™

... just make sure to

    root@debian:~# apt install initscripts console-setup

The following build and install script can be used to configure, build,
install and set up your system to run Finit:

    user@debian:~/finit$ contrib/debian/build.sh

Since `/sbin/init` already exists on your system the script creates
another entry in your GRUB config, in `/etc/grub.d/40_custom`, where
`init=/sbin/finit` is added to your kernel line.  You could of course
also change the `/sbin/init` symlink and uninstall systemd/SysV init if
everything works fine, but that's up to you.

Before rebooting, read up on [/etc/finit.conf](finit.conf) and notice
how `/etc/start.d/` and `/etc/rc.local` can be used to extend the boot
with simpler setup tasks on your system — see the docu for details.

Have fun!  
 /Joachim ツ

[1]: http://without-systemd.org/wiki/index.php/How_to_remove_systemd_from_a_Debian_jessie/sid_installation
[libuEv]: https://github.com/troglobit/libuev
[libite]: https://github.com/troglobit/libite

<!--
  -- Local Variables:
  -- mode: markdown
  -- End:
  -->
