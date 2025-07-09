Files & Layout
==============

Originally Finit was configured using a single file, `/etc/finit.conf`,
and although still possible to use a single configuration file, today
the following layout is recommended:

    /
    |- etc/
    |  |- finit.d/
    |  |   |- available/
    |  |   |  `- my-service.conf
    |  :   |- enabled/
    |  :   |  `- my-service.conf -> ../available/my-service.conf
    |  :   :
    |  :   |- static-service.conf
    |  :   `- another-static.conf
    |  :
    |  `- finit.conf
    |- lib
    |   `- finit/
    |       `- system/
    |           |- 10-hotplug.conf
    |           `- ...
    `- run/
        `- finit/
            `- system/
                |- dbus.conf
                |- keventd.conf
                |- runparts.conf
                |- watchdogd.conf
                `- ...

Configuration files in `/etc` are provided by the user, or projects like
[finit-skel](https://github.com/troglobit/finit-skel) and extended by
the user.

The files in `/lib/finit/system/*.conf` are system critical services and
setup provided by Finit, e.g. udev/mdev that must run very early at system
bootstrap.  This system directory was introduced in Finit v4.4 to replace
the hard-coded services provided by plugins before.  All .conf files in this
directory be either replaced by a system administrator or overridden by a
file with the same name in `/etc/finit.d/`.

The files in `/run/finit/system/*.conf` are created by plugins and Finit
bundled services like runparts, [watchdog](../watchdog.md), and
[`keventd`](../keventd.md) if they are enabled.  Like
`/lib/finit/system/*.conf`, these files can be overridden by file with
the same name in `/etc/finit.d/`.

Services in the `available/` and `enabled/` sub-directories are called
dynamic services, in contrast to static services -- the only difference
being where they are installed and if the `initctl` tool can manage them
with the `enable` and `disable` commands.  An administrator can always
create files and symlinks manually.

At bootstrap, and `initctl reload`, all .conf files are read, starting
with `finit.conf`, then `/lib/finit/system/*.conf`, `finit.d/*.conf`,
and finally all `finit.d/enabled/*.conf` files.  Each directory is a
unique group, where files within each group are sorted alphabetically.

**Example:**

    /lib/finit/system/10-hotplug.conf
    /lib/finit/system/90-testserv.conf
    /run/finit/system/dbus.conf
    /run/finit/system/runparts.conf
    /etc/finit.d/10-abc.conf
    /etc/finit.d/20-abc.conf
    /etc/finit.d/enabled/1-aaa.conf
    /etc/finit.d/enabled/1-abc.conf

The resulting combined configuration is read line by line, each `run`,
`task`, and `service` added to an ordered list that ensures they are
started in the same order.  This is important because of the blocking
properties of the `run` statement.  For an example on the relation of
`service` and `run` statements, and dependency handling between them,
see [Conditional Loading](services.md#conditional-loading), below.

> [!NOTE]
> The names `finit.conf` and `finit.d/` are only defaults.  They can be
> changed at compile-time with two `configure` options:
> `--with-config=/etc/foo.conf` and `--with-rcsd=/var/foo.d`.
>
> They can also be overridden from the [kernel command line](../cmdline.md)
> using: `-- finit.config=/etc/bar.conf` and in that file use the
> top-level configuration directive `rcsd /path/to/finit.d`.

Filesystem Layout
-----------------

Finit is most comfortable with a traditional style Linux filesystem
layout, as specified in the [FHS][]:

    /.
     |- bin/
     |- dev/          # Mounted automatically if devtmpfs is available
     |   |- pts/      # Mounted automatically by Finit if it exists
     |   `- shm/      # Mounted automatically by Finit if it exists
     |- etc/
     |   |- finit.d/
     |   |   |- available/
     |   |   `- enabled/
     |    `- finit.conf
     |- home/
     |- lib/
     |- libexec/
     |- mnt/
     |- proc/         # Mounted automatically by Finit if it exists
     |- root/
     |- run/          # Mounted automatically by Finit if it exists
     |   `- lock/     # Created automatically if Finit mounts /run
     |- sbin/
     |- sys/          # Mounted automatically by Finit if it exists
     |- tmp/          # Mounted automatically by Finit if it exists
     |- usr/
     `- var/
         |- cache/
         |- db/
         |- lib/
         |   `- misc/
         |- lock/
         |- log/
         |- run -> ../run
         |- spool/
         `- tmp/

Finit starts by mounting the critical file systems `/dev`, `/proc/`, and
`/sys`, unless they are already mounted.  When all plugins and other,
core Finit functions, have been set up, all relevant filesystems (where
`PASS > 0`) are checked and mounted from the selected `fstab`, either
the default `/etc/fstab`, or any custom one selected from the command
line, or at build time.

To provide a smooth ride, file system not listed in the given `fstab`,
e.g. `/tmp` and `/run`, are automatically mounted by Finit, as listed
above, provided their respective mount point exists.

With all filesystems mounted, Finit calls `swapon`.

> [!TIP]
> To see what happens when all filesystems are mounted, have a look at
> the [`bootmisc.so` plugin](../plugins.md).

At shutdown, and after having stopped all services and other lingering
processes have been killed, filesystems are unmounted in the reverse
order, and `swapoff` is called.

[FHS]: https://refspecs.linuxfoundation.org/FHS_3.0/fhs/index.html

Managing Services
-----------------

Using `initctl disable my-service` the symlink (above) is removed and
the service is queued for removal.  Several changes can be made to the
system, but it is not until `initctl reload` is called that the changes
are activated.

To add a new static service, drop a `.conf` file in `/etc/finit.d/` and
run `initctl reload`.  (It is also possible to `SIGHUP` PID 1, or call
`finit q`, but that has been deprecated with the `initctl` tool).  Finit
monitors all known active `.conf` files, so if you want to force a
restart of any service you can touch its corresponding `.conf` file in
`/etc/finit.d` and call `initctl reload`.  Finit handles all conditions
and dependencies between services automatically, see the section on
[Service Synchronization](service-sync.md) for more details.

On `initctl reload` the following is checked for all services:

- If a service's `.conf` file has been removed, or its conditions are no
  longer satisfied, the service is stopped.
- If the file is modified, or a service it depends on has been reloaded,
  the service is reloaded (stopped and started).
- If a new service is added it is automatically started — respecting
  runlevels and return values from any callbacks.

For more info on the different states of a service, see the separate
document [Finit Services](../service.md).

Alternate finit.d/
------------------

**Syntax:** `rcsd /path/to/finit.d`

The Finit rcS.d directory is set at compile time with:

    ./configure --with-rcsd=/etc/finit.d

A system with multiple use-cases may be bootstrapped with different
configurations, starting with the kernel command line option:

    -- finit.config=/etc/factory.conf

This file in turn can use the `rcsd` directive to tell Finit to use
another set of .conf files, e.g.:

    rcsd /etc/factory.d

> [!NOTE]
> This directive is only available from the top-level bootstrap .conf
> file, usually `/etc/finit.conf`.

Including Finit Configs
------------------------

**Syntax:** `include <CONF>`

Include another configuration file.  Absolute path required.
