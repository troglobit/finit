Bootstrap
=========

1. Prepare emergency shell, if enabled
2. Set up initial signal handlers
3. Set up default $PATH early
4. Mount `/proc` and `/sys`
5. Check kernel command line for `debug` to figure out log level
6. Load all `.so` plugins
7. Call `fsck` on file systems listed in `/etc/fstab`
8. Populate `/dev` using either udev or mdev, depending on system type
9. Parse `/etc/finit.conf` and all `/etc/finit.d/*.conf` files
10. Start built-in watchdog, if enabled
11. Set hostname
12. Pivot root, or remount `/` read-write, depending on system type
13. Call 1st level hooks, `HOOK_ROOTFS_UP`
14. Mount all file systems listed in `/etc/fstab` and swap, if available
15. Enable SysV init signals
16. Call 2nd level hooks, `HOOK_BASEFS_UP`
17. Cleanup stale files from `/tmp/*` et al, handled by `bootmisc` plugin
18. Load kernel params from `/etc/sysctl.d/*.conf`, `/etc/sysctl.conf`
    et al. (Supports all locations that SysV init does.), handled by
    `procps` plugin
19. Start all 'S' runlevel tasks and services
20. Bring up loopback interface and all `/etc/network/interfaces`, if
    the `.conf` setting `network <SCRIPT>` is set, it is called instead
21. Call 3rd level hooks, `HOOK_NETWORK_UP`
22. If `runparts <DIR>` is set, [run-parts(8)][] is called on `<DIR>`
23. Switch to the configured runlevel from `/etc/finit.conf`, default 2.
    At every runlevel change all `*.conf` files in `/etc/finit.d/` are
    (re)loaded and new services, tasks, and blocking run commands are
    started.  Provided they are allowed in the new runlevel and all of
    their conditions, if any, are set.
24. Call 4th level hooks, `HOOK_SVC_UP`
25. Call `/etc/rc.local`, if it exists and is an executable shell script
26. Call 5th level (last) hooks, `HOOK_SYSTEM_UP`
27. Start all configured TTYs, or a fallback shell on `/dev/console`

In (19) and (22) tasks and services defined in `/etc/finit.conf` and
`/etc/sysctl.d/*.conf` are started.  Remember, all `service` and `task`
stanzas are started in parallel and `run` in sequence, and in the order
listed.  Hence, to emulate a SysV `/etc/init.d/rcS` one could write a
long `finit.conf` with only `run` statements.

Notice the five hook points that are called at various point in the
bootstrap process.  This is where plugins can extend the boot in any way
they please.  There are other hook points, e.g. `HOOK_MOUNT_ERROR`, for
more on this see [plugins.md](plugins.md).

For instance, at `HOOK_BASEFS_UP` a plugin could read an XML file from a
USB stick, convert/copy its contents to the system's `/etc/` directory,
well before all 'S' runlevel tasks are started.  This could be used with
system images that are created read-only and all configuration is stored
on external media.

[run-parts(8)]: http://manpages.debian.org/cgi-bin/man.cgi?query=run-parts
