Finit Features
==============

**Process Supervision**

Start, monitor and restart services should they fail.


**Getty**

Finit supports external getty but also comes with a limited built-in
Getty, useful for really small systems.  A getty sets up the TTY and
waits for user input before handing over to `/bin/login`, which is
responsible for handling the actual authentication.

```conf
tty [12345] /dev/tty1    nowait  linux
tty [12345] /dev/ttyAMA0 noclear vt100
tty [12345] /sbin/getty  -L /dev/ttyAMA0 vt100
```

Users of embedded systems may want to enable automatic serial console
with the special `@console` device.  This works regardless whether the
system uses `ttyS0`, `ttyAMA0`, `ttyMXC0`, or anything else.  Finit
figures it out by querying sysfs: `/sys/class/tty/console/active`.

```conf
tty [12345] @console linux noclear
```

Notice the optional `noclear`, `nowait`, and `nologin` flags.  The
latter is for skipping the login process entirely. For more information,
see the [TTY and Consoles](config/tty.md) section.


**Runlevels**

Support for SysV init-style [runlevels][5] is available, in the same
minimal style as everything else in Finit.  The `[2345]` syntax can be
applied to service, task, run, and TTY stanzas.

Reserved runlevels are 0 and 6, halt and reboot, respectively just like
SysV init.  Runlevel 1 can be configured freely, but is recommended to
be kept as the system single-user runlevel since Finit will not start
networking here.  The configured `runlevel NUM` from `/etc/finit.conf`
is what Finit changes to after bootstrap, unless 'single' (or 'S') is
given on the kernel cmdline, in which case runlevel 1 is started.

All services in runlevel S) are started first, followed by the desired
run-time runlevel.  Run tasks in runlevel S can be started in sequence
by using `run [S] cmd`.  Changing runlevels at runtime is done like any
other init, e.g. <kbd>init 4</kbd>, but also using the more advanced
[`initctl`](initctl.md) tool.


**Conditions**

As mentioned previously, Finit has an advanced dependency system to
handle synchronization, called [conditions](conditions.md).  It can
be used in many ways; depend on another service, network availability,
etc.

One *really cool* example useful for embedded systems is to run certain
scripts if a board has a certain feature encoded in its device tree.  At
bootstrap we run the following `ident` script:

```sh
#!/bin/sh
conddir=/var/run/finit/cond/hw/model
dtmodel=/sys/firmware/devicetree/base/model

if ! test -e $dtmodel; then
    exit 0
fi

model=$(cat $dtmodel | tr "[A-Z] " "[a-z]-")
mkdir -p $conddir && ln -s ../../reconf $conddir/$model
```

Provided the device tree node exists, and is a string, we can then use
the condition `<hw/model/foo>` when starting other scripts.  Here is an
example:

```
run  [S]                /path/to/ident    --
task [2] <hw/model/foo> /path/to/foo-init -- Initializing Foo board
```

> [!TIP]
> Notice the trick with an empty description to hide the call to `ident`
> in the Finit progress output.


**Plugins**

Plugins can *extend* the functionality of Finit and *hook into* the
different stages of the boot process and at runtime.  Plugins are
written in C and compiled into a dynamic library loaded automatically by
finit at boot.  A basic set of plugins are bundled in the `plugins/`
directory.

Capabilities:

- **Hooks**  
  Hook into the boot at predefined points to extend Finit
- **I/O**  
  Listen to external events and control Finit behavior/services

Extensions and functionality not purely related to what an `/sbin/init`
needs to start a system are available as a set of plugins that either
hook into the boot process or respond to various I/O.

For more information, see the [Plugins](plugins.md) section.


**Automatic Reload**

By default, Finit monitors `/etc/finit.d/` and `/etc/finit.d/enabled/`
registering any changes to `.conf` files.  To activate a change the user
must call `initctl reload`, which reloads all modified files, stops any
removed services, starts new ones, and restarts any modified ones.  If the
command line arguments of a service have changed, the process will be
terminated and then started again with the updated arguments. If the arguments
have not been modified and the process supports SIGHUP, the process will
receive a SIGHUP rather than being terminated and started.

For some use-cases the extra step of calling `initctl reload` creates an
unnecessary overhead, which can be removed at build-time using:

    configure --enable-auto-reload


**Cgroups**

Finit supports cgroups v2 and comes with the following default groups in
which services and user sessions are placed in:

     /sys/fs/cgroup
       |-- init/               # cpu.weight:100
       |-- system/             # cpu.weight:9800
       `-- user/               # cpu.weight:100

Finit itself and its helper scripts and services are placed in the
top-level leaf-node group `init/`, which also is _reserved_.

All run/task/service/sysv processes are placed in their own sub-group
in `system/`.  The name of each sub-group is taken from the respective
`.conf` file from `/etc/finit.d`.

All getty/tty processes are placed in their own sub-group in `user/`.
The name of each sub-group is taken from the username.

A fourth group also exists, the `root` group.  It is also _reserved_ and
primarily intended for RT tasks.  If you have RT tasks they need to be
declared as such in their service stanza like this:

    service [...] <...> cgroup.root /path/to/foo args -- description

or

    cgroup.root
    service [...] <...> /path/to/foo args -- description
    service [...] <...> /path/to/bar args -- description

See the [Cgroups](config/cgroups.md) section for more information, e.g.,
how to configure per-group limits.

The `initctl` tool has three commands to help debug and optimize the
setup and monitoring of cgroups.  See the `ps`, `top`, and `cgroup`
commands for details.

> [!NOTE]
> Systems that do not support cgroups, specifically version 2, are
> automatically detected.  On such systems the above functionality is
> disabled early at boot.


[5]: https://en.wikipedia.org/wiki/Runlevel
