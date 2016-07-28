HowTo: Finit on Debian Jessie
=============================

Mini HowTo using Finit to boot Debian GNU/Linux 8.5 (Jessie).  It is
assumed that the user has already installed a compiler, C library header
files, and other tools needed to build a GNU configure based project.

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
users ♥ ♥ ♥ we don't need to worry about `pkg-config`.

> With Debian everything just works!™

    debian:~/finit$ ./configure --enable-rw-rootfs --enable-dbus-plugin \
         --enable-x11-common-plugin --enable-alsa-utils-plugin          \
         --with-random-seed=/var/lib/urandom/random-seed                \
         --with-heading="Debian GNU/Linux 8.5" --with-hostname=debian

Then simply install Finit.

    debian:~/finit$ make && sudo make install

Since `/sbin/init` already exists you need to add another entry to your
GRUB config, in `/etc/grub.d/40_custom`, or add `init=/sbin/finit` to
your kernel line everytime you boot.  Or, you could change the `/sbin`
symlink, but that's on you.

However, before rebooting, make sure to read up on the comments I've
put in [/etc/finit.conf](finit.conf) install it and `start.d/` into
`/etc` on your system — and possibly even check out more of the docu
for details.

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
