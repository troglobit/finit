Finit Signals
=============

Finit is often used on embedded and small Linux systems with BusyBox.
Though Finit comes with its own tools for (poweroff, halt, reboot), for
compatibility with the existing BusyBox toolset the following signals
have been adopted:

* `SIGHUP`  
  Same effect as `finit q`, reloads all *.conf files in `/etc/finit.d/`
* `SIGUSR1`  
  Calls shutdown hooks, including HOOK_SHUTDOWN, stops all running
  processes, and unmounts all file systems.  Then tells kernel to
  halt.  Most people these days want SIGUSR2 though.

  SysV init and systemd use this to re-open their FIFO/D-Bus.
* `SIGUSR2`  
  Like SIGUSR1, but tell kernel to power-off the system, if ACPI
  or similar exists to actually do this.  If the kernel fails
  power-off, Finit falls back to halt.
 
  SysV init N/A, systemd dumps its internal state to log.
* `SIGTERM`  
  Like `SIGUSR1`, but tell kernel to reboot the system when done.
 
  SysV init N/A, systemd rexecutes itself.
* `SIGINT`  
  Sent from kernel when the CTRL-ALT-DEL key combo is pressed.
  SysV init and systemd default to reboot with `shutdown -r`.  
  Finit currently forwards this to `SIGTERM`.
* `SIGPWR`  
  Sent from a power daemon, like `powstatd(8)`, on changes to the
  UPS status.  Traditionally SysV init read /etc/powerstatus and
  acted on "OK", "FAIL", or "LOW" and then removed the file.  
  Finit currently forwards this to `SIGUSR2`.

<!--
  -- Local Variables:
  -- mode: markdown
  -- End:
  -->
