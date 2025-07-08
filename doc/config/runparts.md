Run-parts Scripts
-----------------

**Syntax:** `runparts [progress] [sysv] <DIR>`

Call [run-parts(8)][] on `DIR` to run start scripts.  All executable
files in the directory are called, in alphabetic order.  The scripts in
this directory are executed at the very end of runlevel `S`.

A common use-case for runparts scripts is to create and enable/disable
services, which Finit will then apply when changing runlevel from S to
whatever the next runlevel is set to be (default 2).  E.g., generate a
`/etc/chrony.conf` and call `initctl enable chronyd`.

**Options:**

 - `progress`: display the progress of each script being executed
 - `sysv`: run only SysV style scripts, i.e., `SNNfoo`, or `KNNbar`,
   where `NN` is a number (0-99).

If global debug mode is enabled, the `runparts` program is also called
with the debug flag.

**Limitations:**

Scripts called from `runparts`, or hook scripts (see below), are limited
in their interaction with Finit.  Like the standalone `run` stanza and
the `/etc/rc.local` shell script, Finit waits for their completion
before continuing.  None of them can issue commands to start, stop, or
restart other services.  Also, ensure all your services and programs
either terminate or start in the background or you will block Finit.

> [!NOTE]
> `runparts` scripts are only read and executed in runlevel S.  See
> [hook scripts](../plugins.md#hooks) for other ways to run scripts at
> certain points during the complete lifetime of the system.

**Recommendations:**

It can be beneficial to use `01-name`, `02-othername`, etc., to ensure
the scripts are started in that order, e.g., if there is a dependency
order between scripts.  Symlinks to existing daemons can talso be used,
but make sure they daemonize (background) themselves properly, otherwise
Finit will lock up.

If `S[0-9]foo` and `K[0-9]bar` style naming is used, the executable will
be called with an extra argument, `start` and `stop`, respectively.
E.g., `S01foo` will be called as `S01foo start`.  Of course, `S01foo`
and `K01foo` may be a symlink to to `another/directory/foo`.

[run-parts(8)]: http://manpages.debian.org/cgi-bin/man.cgi?query=run-parts
