SysV Init Compatibility
=======================

It is not possible to run unmodified SysV init systems with Finit.  This
was never the intention and is not the strength of Finit.  However, it
comes with a few SysV Init compatibility features to ease the transition
from a serialized boot process.

SysV Init Scripts
-----------------

**Syntax:** `sysv [LVLS] <COND> /path/to/init-script -- Optional description`

> `<COND>` is described in the [Services](services.md) section.

Similar to `task` is the `sysv` stanza, which can be used to call SysV
style scripts.  The primary intention for this command is to be able to
reuse much of existing setup and init scripts in Linux distributions.
  
When entering an allowed runlevel, Finit calls `init-script start`, when
entering a disallowed runlevel, Finit calls `init-script stop`, and if
the Finit .conf, where `sysv` stanza is declared, is modified, Finit
calls `init-script restart` on `initctl reload`.  Similar to how
`service` stanzas work.

Forking services started with `sysv` scripts can be monitored by Finit
by declaring the PID file to look for: `pid:!/path/to/pidfile.pid`.
Notice the leading `!`, it signifies Finit should not try to create the
file, but rather watch that file for the resulting forked-off PID.  This
syntax also works for forking daemons that do not have a command line
option to run it in the foreground, more on this below in `service`.

> [!TIP]
> See also [SysV Init Compatibility](#sysv-init-compatibility).

`runparts DIRECTORY`
--------------------

For a directory with traditional start/stop scripts that should run, in
order, at bootstrap, Finit provides the `runparts` directive.  It runs
in runlevel S, at the very end of it (before calling `/etc/rc.local`)
making it perfect for most scenarios.

For syntax details, see the [Run-parts Scripts](runparts.md) section.
Here is an example take from a Debian installation:

    runparts /etc/rc2.d

Files in these directories are usually named `SNNfoo` and `KNNfoo`,
which Finit knows about and automatically appends the correct argument:

    /bin/sh -c /etc/rc2.d/S01openbsd-inetd start

or

    /bin/sh -c /etc/rc0.d/K01openbsd-inetd stop

Files that do not match this pattern are started similarly but without
the extra command line argument.

Start/Stop Scripts
------------------

For syntax details, see [SysV Init Scripts](#sysv-init-scripts), above.
Here follows an example taken from a Debian installation:

    sysv [2345] <pid/syslogd> /etc/init.d/openbsd-inetd -- OpenBSD inet daemon

The init script header could be parsed to extract `Default-Start:` and
other parameters for the `sysv` command to Finit.  There is currently no
way to detail a generic syslogd dependency in Finit, so `Should-Start:`
in the header must be mapped to the condition system in Finit using an
absolute reference, here we depend on the sysklogd project's syslogd.

`/etc/rc.local`
---------------

One often requested feature, early on, was a way to run a site specific
script to set up, e.g., static routes or firewall rules.  A user can add
a `task` or `run` command in the Finit configuration for this, but for
compatibility reasons the more widely know `/etc/rc.local` is used if
it exists, and is executable.  It is called very late in the boot process
when the system has left runlevel S, stopped all old and started all new
services in the target runlevel (default 2).

> In Finit releases before v4.5 this script blocked Finit execution and
> made it as good as impossible to call `initctl` during that time.

`init q`
--------

When `/sbin/finit` is installed as `/sbin/init`, it is possible to use
`init q` to reload the configuration.  This is the same as calling
`initctl reload`.
