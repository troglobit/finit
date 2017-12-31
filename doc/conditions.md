Finit Conditions
================

![The service state machine](svc-machine.png "The service state machine")

* [Introduction](#introduction)
* [Triggering](#triggering)
* [Built-in Conditions](#built-in--conditions)
* [Debugging](#debugging)
* [Internals](#internals)


Introduction
------------

Conditions are a new addition to Finit, introduced in v3, with the
intention of providing a mechanism for common synchronization problems.
For example:

- *"do not start this service until another process has started"*, or
- *"do not starty until basic network access is available"*

Conditions are similar in syntax to declaring runlevels per service.
They are specified within angle brackets `<>` and can be applied to any
of the `service`, `task`, or `run` stanza.  Multiple conditions may be
specified separated by comma.  Multiple conditions are logically AND'ed
during evaluation, i.e. all conditions must be satisfied in order for a
service to run.


### Example

```shell
    service [2345] <svc/sbin/setupd,svc/sbin/zebra> /sbin/netd -- Network monitor
```

In this example the Network monitor daemon `netd` is not started until
both the `svc/sbin/setupd` *and* `svc/sbin/zebra` conditions are
satisfied.  An `svc` condition is satisfied by the corresponding
service's PID file being created.

**NOTE:** Conditions also stop services when a condition is no longer
  asserted.  I.e., if the Zebra process above stops or restarts, netd
  will also stop or restart.


Triggering
----------

Conditions are triggered by built-in plugins, but can for debugging
purposes also be controlled using the `cond set` and `cond clear`
sub-commands to the `initctl` control tool:

* `initctl cond set your/cond/here`

  To set a condition

* `initctl cond clear your/cond/here`

  To clear a condition

Conditions retain their current state until the next reconfiguration or
runlevel change.  At that point all set conditions transition into the
`flux` state, meaning the condition's state is unknown.  (For more info
on this, see [Internals](#internals).)  Thus, after a reconfiguration it
is up to the "owner" of the condition to convey the new (or possibly
unchanged) state of it.


Built-in Conditions
-------------------

Finit is distributed with a `pidfile` and `netlink` plugin.  If enabled,
the `pidfile` plugin watches `/var/run/` for PID files created by the
monitored services, and sets a corresponding condition in the `svc/`
namespace.  Similarily, the `netlink` plugin provides basic conditions
for when an interface is brought up/down and when a default route
(gateway) is set, in the `net/` namespace.

With the example listed above, finit does not start the `/sbin/netd`
daemon until `setupd` and `zebra` has started *and* created their PID
files.  Which they do when they have completed their initial set up and
are ready to receive signals.

The full path to the dependency is needed by finit to match the PID file
to a monitored process.  Finit expects monitored services to touch their
PID files, i.e. update the mtime, when they reload their configuration
files after a `SIGHUP`.  Some services do not support `SIGHUP` and are
instead restarted, which is a crude but effective way to have the PID
file touched (re-created).

Built-in conditions:

- `svc/<PATH>`
- `net/route/default`
- `net/<IFNAME>/exist`
- `net/<IFNAME>/up`
- `net/<IFNAME>/running`

**Note:** `up` means administratively up, the interface flag `IFF_UP`.
  `running` is the `IFF_RUNNING` flag, meaning operatively up.  The
  difference is that `running` tells if the NIC has link.


Debugging
---------

If a service is not being started as it should, the problem might be
that one of its conditions is not in the expected state.  Use the
command `initctl status` to inspect service status.  Services in the
`ready` state are pending a condition.

In that situation, running `initctl cond show` reveals which of the
conditions that are not satisfied.  Listed as `off` below.

**Example:**

```shell
    ~ # initctl cond show
    PID     Service               Status  Condition (+ on, ~ flux, - off)
    ===============================================================================
    1419    /sbin/netd            on      <+svc/sbin/setupd,+svc/sbin/zebra>
    0       /sbin/udhcpc          off     <-net/vlan1/exist>
```

Here we can see that `netd` is allowed to run since both its conditions
are in the `on` state, as indicated by the `+`-prefix.  `udhcpc` however
is not allowed to run since `net/vlan1/exist` condition is not satsifed.
As indicated by the `-`-prefix.

To fake interface `vlan1` suddenly appearing, and test what happens to
`udhcpc` we can enable debug mode and assert the condition, like this:

```shell
    ~ # initctl debug
    ~ # initctl cond set net/vlan1/exist
```

Then watch the console for the debug messages and then check the output
from `initctl cond show` again.  (The client will likely have failed to
start, but at least the condition is now satisfied.)

There is also the `initctl cond dump` command, which dumps all known
conditions and their current status.


Internals
---------

Conditions are implemented as simple files in the file system, in the
`/var/run/finit/cond/` sub-directory.  Use the `initctl cond set/clear`
commands to add/remove files in this hierarchy.

A condition is always in one of three states:

* `  on` (+): The condition is asserted.
* ` off` (-): The condition is deasserted.
* `flux` (~): The conditions state is unknown.

All conditions that have not explicitly been set are interpreted as
being in the `off` state.

When a reconfiguration is requested, Finit transitions all conditions to
the `flux` state.  As a result, services that depend on a condition are
sent `SIGSTOP`.  Once the new state of the condition is asserted, the
service receives `SIGCONT`.  If the condition is no longer satisfied the
service will then be stopped, otherwise no further action is taken.

This STOP/CONT handling minimizes the number of unnecessary service
restarts that would otherwise occur because a depending service was sent
`SIGHUP` for example.

Therefore, any plugin that supplies Finit with conditions must ensure
that their state is updated after each reconfiguration.  This can be
done by binding to the `HOOK_SVC_RECONF` hook.  For an example of how
to do this, see `plugins/pidfile.c`.
