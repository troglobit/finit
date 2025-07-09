Requirements
============

Finit is capable of running on both desktop/server systems with udev and
embedded systems that usually come with BusyBox mdev.  Some systems have
systemd-udev or eudev today instead of the original udev, Finit probes
for all of them at runtime and expects `/dev/` to be a writable file
system using `devtmpfs`.  It is also possible to run on a statically set
up `/dev` if needed.  It is however not a good idea to have both udev
and mdev installed at the same time, this will lead to unpredictable
results.

At boot Finit calls either `mdev` or `udevd` to populate `/dev`, this is
done slightly differently and on systems with udev you might want to add
the following one-shot task early in your `/etc/finit.conf`:

```conf
run [S] udevadm settle --timeout=120 -- Waiting for udev
```

Finit has a built-in Getty for TTYs, but requires a working `/bin/login`
or `/bin/sh`, if no TTYs are configured in `/etc/finit.conf`.

For a fully operational system `/var`, `/run` and `/tmp` must be set up
properly in `/etc/fstab` -- which is iterated over at boot.
