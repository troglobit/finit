# Services

## State Machine

![A service's state machine](svc-machine.png "A service's state machine")

A service is bound to a state machine that is in one of six (6)
states. For `run`s and `task`s there is an additional end state called
`DONE`, not shown in the diagram for brevity. All services start in
the `HALTED` state.

The current state depends on the two following conditions:

* `E`: Service enabled. In order for `E` to be satisfied, the service
  must be allowed to run in the current runlevel and must not be
  blocked. A service may be blocked for several reasons:

  - The user has manually stopped the service using `initctl stop
    JOB`.

  - The binary repeatedly dies after being started. I.e. keeps
    crashing.

  - The binary is missing in the filesystem.

* `C`: Service conditions are satisfied.
