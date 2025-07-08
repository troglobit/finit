Service Synchronization
=======================

Finit was created for fast booting systems.  Faster than a regular SysV
Init based system at the time.  Early on the need for a guaranteed start
order of services (daemons) arose.  I.e., service `A` must be guaranteed
to have started (and be ready!) before `B`.  The model that was chosen
to determine this was very simple: PID files.

Early on in UNIX daemons were controlled with basic IPC like signals,
and the way for a user to know that a daemon was ready to respond to
signals (minimally having set up its signal handler), was to tell the
user;

> "Hey, you can send signals to me using the PID in this file:
> `/var/run/daemon.pid`".

Since most systems run fairly unchanged after boot, Finit could rely on
the PID file for `A` being created before launching `B`.  This method
has worked well for a long time, and for systems based on Open Source it
was easy to either add PID file support to a daemon without support for
it, or fix ordering issues (PID file created before signal handler is
set up) in existing daemons.

However, with the advent of other Init systems (Finit is rather old),
most notably systemd and s6, other methods for signaling "readiness"
arrived and daemons were adapted to these new schemes to a larger
extent.

As of Finit v4.4 partial support for systemd and s6 style readiness
notification is available, and the native PID file mode of operation is,
as of Finit v4.6 optional, by default it is still enabled, but this can
be changed in `finit.conf`:

    readiness none

This will be made the default in Finit 5.0.  In this mode of operation,
every service needs to explicitly declare their readiness notification,
like this:

    service notify:pid     watchdogd
    service notify:systemd foo
    service notify:s6      bar
    service notify:none    qux

The `notify:none` syntax is for completeness in systems which run in
`readiness pid` mode (default).  Services declared with `notify:none`
will transition to ready as soon as Finit has started them, e.g.,
`service/qux/ready`.

To synchronize two services the following condition can be used:

    service notify:pid                 watchdogd
    service <service/watchdogd/ready>  stress-ng --cpu 8

For details on the syntax and options, see below.

> [!NOTE]
> On `initctl reload` conditions are set in "flux", while figuring out
> which services to stop, start or restart.  Services that need to be
> restarted have their `ready` condition removed before Finit issue a
> SIGHUP (if they support that), or stop-starting them.  A daemon is
> expected to reassert its readiness, e.g. systemd style daemons to
> write `READY=1\n`.
>
> However, the s6 notify mode does not support this because in s6 you
> are expected to close your notify descriptor after having written
> `\n`.  This means s6 style daemons currently must be stop-started.
> (Declare the service with `<!>` in its condition statement.)
>
> For default, PID file style readiness notification, daemons are
> expected to either create their PID files, or touch it using
> `utimensat()` to reassert readiness.  Triggering both the `<pid/>`
> and `<.../ready>` conditions.
