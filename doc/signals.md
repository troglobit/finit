Finit Signals
=============

Finit is often used on embedded and small Linux systems with BusyBox.
Though Finit comes with its own tools for (poweroff, halt, reboot), for
compatibility with the existing BusyBox toolset the following signals
have been adopted:

`SIGHUP`
--------

Same effect as `finit q`, `init q`, or `initctl reload`, reloads all
*.conf files in `/etc/finit.d/`

This also restarts the API (initctl) socket, like SysV init and systemd
does on USR1 with their FIFO/D-Bus.


`SIGUSR1`
---------

Since Finit 4.1 this signal causes Finit to restart its API (initctl)
socket, like SysV init and systemd does on USR1 with their FIFO/D-Bus.

Finit <= 4.0 performed a system halt (like USR2 without power-off), but
this caused compatibility problems with systemd and sysvinit on desktop
systems.  Hence, since Finit 4.1 it is no longer possible to halt a
system with a signal.


`SIGUSR2`
---------

Calls shutdown hooks, including `HOOK_SHUTDOWN`, stopping all running
processes, and unmounts all file systems.  Then tells kernel to power
off the system, if ACPI or similar exists to actually do this.  If the
kernel fails power-off, Finit falls back to halt.

SysV init N/A, systemd dumps its internal state to log.

`SIGTERM`
---------

Like `SIGUSR2`, but tell kernel to reboot the system when done.
 
SysV init N/A, systemd rexecutes itself.

`SIGINT`
--------

Sent from kernel when the CTRL-ALT-DEL key combo is pressed.  SysV init
and systemd default to reboot with `shutdown -r`.

Finit currently forwards this to `SIGTERM`.

`SIGPWR`
--------

Sent from a power daemon, like `powstatd(8)`, on changes to the
UPS status.  Traditionally SysV init read /etc/powerstatus and
acted on "OK", "FAIL", or "LOW" and then removed the file.  
Finit currently forwards this to `SIGUSR2`.

