# Finit Conditions

In addition to runlevels, Finit supports user-defined conditions that
services can declare a dependency on. The conditions are specified in
angle brackets on the service stanza. Multiple conditions may be
specified, separated by commas. Conditions are AND-ed together during
evaluation, i.e. all conditions must be satisfied in order for the
service to be able to run.

##### Example:

```
service  [2345]  <svc/sbin/setupd,svc/sbin/zebra>  /sbin/netd  -- Network monitor
```

In this example the `netd` daemon will not be started until both
`svc/sbin/setupd` and `svc/sbin/zebra` are satisfied.


## Triggering

Conditions are triggered by using the `emit` sub-command of the
`initctl` command.

   * To set a condition, use: `initctl emit +your/cond/here`.
   * To clear a condition, use: `initctl emit -your/cond/here`.

A condition will retain its current state until the next
reconfiguration or runlevel change. At that point, all set conditions
will transition into the `flux` state, meaning that the condition's
state is unknown. The rationale for this can be found in the
[Internals](#internals) section. Thus, after a reconfiguration, it is
up to the "owner" of the condition to convey the new (or possibly
unchanged) state of it.


## Built-in Conditions

Finit is distributed with the `pidfile`-plugin. If enabled, it will
watch `/var/run/` for pidfiles created by services that is controls
and set a corresponding condition in the `svc/` namespace.

Thus, if Finit starts the `/sbin/netd` daemon and it creates
`/var/run/netd.pid`, the condition `svc/sbin/netd` will be set. If the
file is removed, the condition will be cleared.


## Debugging

If a service is not being started when it ought to be, the problem
might be that one of its conditions are not in the expected
state. This is indicated when running `initctl status` by the service
being in the `ready` state.

In that situation, running `initctl cond show` will reveal which of
the conditions that are not in the `on` state.

##### Example:

```
~ # initctl cond show
PID     Service               Status  Condition (+ on, ~ flux, - off)
====================================================================================
1419    /sbin/netd            on      <+svc/sbin/setupd,+svc/sbin/zebra>
0       /sbin/udhcpc          off     <-net/vlan1/exist>
```

Here we can see that `netd` is allowed to run since both of its
conditions are in the `on` state, as indicated by the
`+`-prefix. `udhcpc` is not allowed to run since `net/vlan1/exist` is
in the `off` state, indicated by the `-`-prefix.


## Internals

A condition is always in one of three states:

   * `on`: The condition is asserted.
   * `off`: The condition is deasserted.
   * `flux`: The conditions state is unknown.

All conditions that have not explicitly been set are interpreted as
being in the `off` state.

When a reconfiguration is requested, Finit will transition all
conditions to the `flux` state. As a result, any service that depends
on the condition will be sent a SIGSTOP. Once the new state of the
condition becomes known the service will receive a SIGCONT. Then if
the condition is no longer satisfied the service will be stopped,
otherwise no action needs to be taken.

This minimizes the number of services that have to be restarted
needlessly, just because the depending service was sent a SIGHUP for
example.

Therefore, any plugin that supplies Finit with conditions must ensure
that their state is updated after each reconfiguration. This can be
done by binding to the `HOOK_SVC_RECONF`-hook. Look at
`plugins/pidfile.c` too see an example of this.
