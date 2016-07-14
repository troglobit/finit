Finit Conditions
================

![Condition state machine](../images/cond-statem.jpg "Condition state machine")


Table of Contents
-----------------

* [Introduction](#introduction)
* [Triggering](#triggering)
* [Built-in Conditions](#built-in--conditions)
* [Debugging](#debugging)
* [Internals](#internals)


Introduction
------------

In addition to runlevels, services can declare user-defined conditions
as dependencies.  Conditions are specified within angle brackets (<>) in
the service stanza.  Multiple conditions may be specified separated by
comma.  Conditions are AND'ed during evaluation, i.e. all conditions
must be satisfied in order for a service to run.


**Example:**

```shell
    service [2345] <svc/sbin/setupd,svc/sbin/zebra> /sbin/netd -- Network monitor
```

In this example the Network monitor daemon `netd` is not started until
both the `svc/sbin/setupd` *and* `svc/sbin/zebra` conditions are
satisfied.  An `svc` condition is satisfied by the corresponding
service's pidfile being created.


Triggering
----------

Conditions are triggered by using the `emit` command of the `initctl`
control tool.

* `initctl emit +your/cond/here`

  To set a condition

* `initctl emit -your/cond/here`

  To clear a condition

Conditions retain their current state until the next reconfiguration or
runlevel change.  At that point all set conditions transition into the
`flux` state, meaning the condition's state is unknown.  (See
[Internals](#internals) for the rationale behind this.)  Thus, after a
reconfiguration it is up to the "owner" of the condition to convey the
new (or possibly unchanged) state of it.


Built-in Conditions
-------------------

Finit is distributed with the `pidfile`-plugin. If enabled, it will
watch `/var/run/` for pidfiles created by services that it controls
and set a corresponding condition in the `svc/` namespace.

For example, if Finit starts the `/sbin/netd` daemon and it creates the
file `/var/run/netd.pid`, the condition `svc/sbin/netd` is satisfied.
If the file is removed, the condition is cleared.


Debugging
---------

If a service is not being started as it should, the problem might be
that one of its conditions are not in the expected state.  Use the
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


Internals
---------

A condition is always in one of three states:

* `on`: The condition is asserted.
* `off`: The condition is deasserted.
* `flux`: The conditions state is unknown.

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

<!--
  -- Local Variables:
  -- mode: markdown
  -- End:
  -->
