Rescue Mode
===========

Finit supports a rescue mode which is activated by the `rescue` option
on the kernel command line.  See [cmdline docs](../cmdline.md) for how to
activate it.

This rescue mode can be disabled at configure time using:

    configure --without-rescue

The rescue mode comes in two flavors; *traditional* and *fallback*.

> [!NOTE]
> In this mode `initctl` will not work.  Use the `-f` flag to force
> `reboot`, `shutdown`, or `poweroff`.

Traditional
-----------

This is what most users expect.  A very early maintenance login prompt,
served by the system `sulogin` program from util-linux, or BusyBox.  If
that is not found in `$PATH`, the bundled `/libexec/finit/sulogin`
program is used instead.  If a successful login is made, or if the user
exits (Ctrl-D), the rescue mode is ended and the system boots up
normally.

> [!WARNING]
> The bundled sulogin in Finit can at configure time be given another
> user than the default (root).  If the sulogin user does not have a
> password, or __the account is locked__, the user is presented with a
> prompt: `"Press enter to enter maintenance mode."`, which will open
> up a root shell *without prompting for password*!

Fallback
--------

If no `sulogin` program is found, Finit tries to bring up as much of its
own functionality as possible, yet limiting many aspects, meaning; no
network, no `fsck` of file systems in `/etc/fstab`, no `/etc/rc.local`,
no `runparts`, and most plugins are skipped (except those that provide
functionality for the condition subsystem).

Instead of reading `/etc/finit.conf` et al, system configuration is read
from `/lib/finit/rescue.conf`, which can be freely modified by the
system administrator.

The bundled default `rescue.conf` contains nothing more than:

    runlevel 1
    tty [12345] rescue

The `tty` has the `rescue` option set, which works similar to the board
bring-up tty option `notty`.  The major difference being that `sulogin`
is started to query for root/admin password.  If `sulogin` is not found,
`rescue` behaves like `notty` and gives a plain root shell prompt.

If Finit cannot find `/lib/finit/rescue.conf` it defaults to:

    tty [12345] rescue

There is no way to exit the *fallback* rescue mode.