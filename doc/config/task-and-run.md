run (sequence)
--------------

**Syntax:** `run [LVLS] <COND> /path/to/cmd ARGS -- Optional description`

> `<COND>` is described in the [Services](services.md) section.

One-shot command to run in sequence when entering a runlevel, with
optional arguments and description.  `run` commands are guaranteed to be
completed before running the next command.  Useful when serialization is
required.

> [!WARNING]
> Try to avoid the `run` command.  It blocks much of the functionality
> in Finit, like (re)starting other (perhaps crashing) services while a
> `run` task is executing.  Use other synchronization mechanisms
> instead, like conditions.

Incomplete list of unsupported `initctl` commands in `run` tasks:

 - `initctl runlevel N`, setting runlevel
 - `initctl reboot`
 - `initctl halt`
 - `initctl poweroff`
 - `initctl suspend`

To prevent `initctl` from calling Finit when enabling and disabling
services from inside a `run` task, use the `--force` option.  See
also the `--quiet` and `--batch` options.

task (parallel)
---------------

**Syntax:** `task [LVLS] <COND> /path/to/cmd ARGS -- Optional description`

> `<COND>` is described in the [Services](services.md) section.

One-shot like 'run', but starts in parallel with the next command.
  
Both `run` and `task` commands are run in a shell, so basic pipes and
redirects can be used:

    task [s] echo "foo" | cat >/tmp/bar

Please note, `;`, `&&`, `||`, and similar are *not supported*.  Any
non-trivial constructs are better placed in a separate shell script.
