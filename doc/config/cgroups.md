There are three major cgroup configuration directives:

 1. Global top-level group: `init`, `system`, `user`, or a custom group
 2. Selecting a top-level group for a set of run/task/services
 3. Per run/task/service limits

> [!NOTE]
> Linux cgroups and details surrounding values are not explained in the
> Finit documentation.  The Linux admin-guide cover this well:
> <https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html>

Top-level Group Configuration
-----------------------------

    # Top-level cgroups and their default settings.  All groups mandatory
    # but more can be added, max 8 groups in total currently.  The cgroup
    # 'root' is also available, reserved for RT processes.  Settings are
    # as-is, only one shorthand 'mem.' exists, other than that it's the
    # cgroup v2 controller default names.
    cgroup init   cpu.weight:100
    cgroup user   cpu.weight:100
    cgroup system cpu.weight:9800

Adding an extra cgroup `maint/` will require you to adjust the weight of
the above three.  We leave `init/` and `user/` as-is reducing weight of
`system/` to 9700.

    cgroup system cpu.weight:9700

    # Example extra cgroup 'maint'
    cgroup maint  cpu.weight:100

By default, the `system/` cgroup is selected for almost everything.  The
`init/` cgroup is reserved for PID 1 itself and its closest relatives.
The `user/` cgroup is for local TTY logins spawned by getty.

Selecting Cgroups
------------------

To select a different top-level cgroup, e.g. `maint/`, one can either
define it for a group of run/task/service directives in a `.conf` or per
each stanza:

    cgroup.maint
    service [...] <...> /path/to/foo args -- description
    service [...] <...> /path/to/bar args -- description

or

    service [...] <...> cgroup.maint /path/to/foo args -- description

The latter form also allows per-stanza limits on the form:

    service [...] <...> cgroup.maint:cpu.max:10000,mem.max:655360 /path/to/foo args -- description

Notice the comma separation and the `mem.` exception to the rule: every
cgroup setting maps directly to cgroup v2 syntax.  I.e., `cpu.max` maps
to the file `/sys/fs/cgroup/maint/foo/cpu.max`.  There is no filtering,
except for expanding the shorthand `mem.` to `memory.`, if the file is
not available, either the cgroup controller is not available in your
Linux kernel, or the name is misspelled.

A daemon using `SCHED_RR` currently need to run outside the default cgroups.

    service [...] <...> cgroup.root /path/to/daemon arg -- Real-Time process
