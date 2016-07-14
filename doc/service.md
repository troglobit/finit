Finit Services
==============

![The service state machine](svc-machine.png "The service state machine")

State Machine
-------------

A service is bound to a state machine that is in one of six (6) states.
For `run`s and `task`s there is an additional end state called `DONE`,
not shown in the diagram for brevity.  Services start in the `HALTED`
state.

The current state depends on the two following conditions:

* `E`: Service enabled. In order for `E` to be satisfied, the service
  must be allowed to run in the current runlevel and must not be
  blocked.  A service may be blocked for several reasons:

  - The user has manually stopped the service using `initctl stop JOB`
  - The binary repeatedly dies after being started. I.e. keeps crashing.
  - The binary is missing in the filesystem.

* `C`: Service conditions are satisfied:

  - `on` (+): The condition is asserted.
  - `off` (-): The condition is deasserted.
  - `flux` (~): The conditions state is unknown.

For a detailed description of conditions, and how to debug them,
see the [Finit Conditions](conditions.md) document.

<!--
  -- Local Variables:
  -- mode: markdown
  -- End:
  -->
