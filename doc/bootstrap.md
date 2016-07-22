Bootstrap
=========

1. Populate `/dev`
2. Parse `/etc/finit.conf`
3. Load all `.so` plugins
4. Remount/Pivot `/` to get R+W
5. Call 1st level hooks, `HOOK_ROOTFS_UP`
6. Mount `/etc/fstab` and swap, if available
7. Cleanup stale files from `/tmp/*` et al
8. Enable SysV init signals
9. Call 2nd level hooks, `HOOK_BASEFS_UP`
10. Start all 'S' runlevel tasks and services
11. Load kernel parameters from `/etc/sysctl.conf`
12. Set hostname and bring up loopback interface
13. Call `network` script, if set in `/etc/finit.conf`
14. Call 3rd level hooks, `HOOK_NETWORK_UP`
15. Switch to the configured runlevel from `/etc/finit.conf`, default 2.
    At every runlevel change all `*.conf` files in `/etc/finit.d/` are
    (re)loaded and new services, tasks, and blocking run commands are
    started.  Provided they are allowed in the new runlevel and all of
    their conditions, if any, are set.
16. Call 4th level hooks, `HOOK_SVC_UP`
17. If `runparts <DIR>` is set, [run-parts(8)][] is called on `<DIR>`
18. Call `/etc/rc.local`, if it exists and is an executable shell script
19. Call 5th level (last) hooks, `HOOK_SYSTEM_UP`
20. Start TTYs defined in `/etc/finit.conf`, or rescue on `/dev/console`

In (10) and (15) tasks and services defined in `/etc/finit.conf` are
started.  Remember, all `service` and `task` stanzas are started in
parallel and `run` in sequence, and in the order listed.  Hence, to
emulate a SysV `/etc/init.d/rcS` one could write a long file with only
`run` statements.

Notice the five hook points that are called at various point in the
bootstrap process.  This is where plugins can extend the boot in any way
they please.

For instance, at `HOOK_BASEFS_UP` a plugin could read an XML file from a
USB stick, convert/copy its contents to the system's `/etc/` directory,
well before all 'S' runlevel tasks are started.  This could be used with
system images that are created read-only and all configuration is stored
on external media.


[run-parts(8)]: http://manpages.debian.org/cgi-bin/man.cgi?query=run-parts

<!--
  -- Local Variables:
  -- mode: markdown
  -- End:
  -->
