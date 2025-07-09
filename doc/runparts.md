Runparts & `/etc/rc.local`
==========================

At the end of the boot, when all bootstrap (`S`) tasks and services have
started, but not networking, Finit calls its built-in [run-parts(8)][]
command on any configured `runparts <DIR>` directory.  This happens just
before changing to the configured runlevel (default 2).  (Networking is
enabled just prior to changing from single user mode.)

```shell
runparts /etc/rc.d/
```

Right after the runlevel change when all services have started properly,
`/etc/rc.local` is called.

No configuration stanza in `/etc/finit.conf` is required for `rc.local`.
If it exists and is an executable shell script Finit calls it at the very
end of the boot, before calling the `HOOK_SYSTEM_UP`.  See more in the
[Hook Scripts](plugins.md#hooks) section.


### Limitations

It is not possible to call Finit via signals or use `initctl` in any
runparts or `/etc/rc.local` script.  This because Finit is single
threaded and is calling these scripts in a blocking fashion at the end
of runlevel S, at which point the event loop has not yet been started.

The event loop is the whole thing which Finit is built around, except
for runlevel S, which remains a slow procession through a lot of set up,
with a few hooks and blocking call outs to external scripts.

However, not all `initctl` commands are prohibited. Supported commands:

 - `initctl cond`: only operate on files in `/run/finit/cond`
 - `initctl enable/disable`: enabled run/task/service is activated on
   the runlevel change from S to 2
 - `initctl touch/show/create/delete/list`: `create`, provided the
   non-interactive mode is used, again changes take effect in the
   runlevel change directly after bootstrap
 - `initctl -f reboot/poweroff/halt`: provided the `-f` flag is used to
   force direct kernel commands

> [!TIP]
> you can set a `usr/` condition in `/etc/rc.local` and have a
> service/task in runlevel 2 depend on it to execute.

[run-parts(8)]:     https://manpages.debian.org/cgi-bin/man.cgi?query=run-parts
