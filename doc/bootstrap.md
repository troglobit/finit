Bootstrap
=========

0. Get bearings, disable kernel logs, initialize TTY and print banner,
   unless `HOOK_BANNER` is defined, then call that instead
1. Prepare emergency shell, if enabled
2. Set up initial signal handlers
3. Set up default $PATH early
4. Mount `/proc` and `/sys`
5. Check kernel command line for `debug` to figure out log level
6. Load all `.so` plugins
7. Call `fsck` on file systems listed in `/etc/fstab`
8. Populate `/dev` using either udev or mdev, depending on system type
9. Parse `/etc/finit.conf`
10. Start built-in watchdog, if enabled and `WDT_DEVNODE` exists.  This
    means any WDT that requires a kernel module need to be either
    compiled into the kernel, or insmod'ed in `/etc/finit.conf`
11. Load all `/etc/finit.d/*.conf` files and set hostname
12. Remount `/` read-write if `/` is listed in `/etc/fstab` without `ro`
13. Call 1st level hooks, `HOOK_ROOTFS_UP`
14. Mount all file systems listed in `/etc/fstab` and swap, if available.
    On mount error `HOOK_MOUNT_ERROR` is called.  After mount, regardless
    of error, `HOOK_MOUNT_POST` is called
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
27. Start all configured TTYs

In (19) and (23) tasks and services defined in `/etc/finit.conf` and
`/etc/finit.d/*.conf` are started.

Notice the seven hook points that are called at various point in the
bootstrap process.  This is where plugins can extend the boot in any
way they please.  There are other hook points available, for more on
this, see [plugins.md](plugins.md).

For instance, at `HOOK_BASEFS_UP` a plugin could read an XML file from a
USB stick, convert/copy its contents to the system's `/etc/` directory,
well before all 'S' runlevel tasks are started.  This could be used with
system images that are created read-only and all configuration is stored
on external media.

[run-parts(8)]: http://manpages.debian.org/cgi-bin/man.cgi?query=run-parts
