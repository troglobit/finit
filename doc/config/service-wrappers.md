Service Wrapper Scripts
=======================

If your service requires to run additional commands, executed before the
service is actually started, like the systemd `ExecStartPre`, you can
use a wrapper shell script to start your service.

The Finit service `.conf` file can be put into `/etc/finit.d/available`,
so you can control the service using `initctl`.  Then use the path to
the wrapper script in the Finit `.conf` service stanza.  The following
example employs a wrapper script in `/etc/start.d`.

**Example:**

* `/etc/finit.d/available/program.conf`:

        service [235] <!> /etc/start.d/program -- Example Program

* `/etc/start.d/program:`

        #!/bin/sh
        # Prepare the command line options
        OPTIONS="-u $(cat /etc/username)"

        # Execute the program
        exec /usr/bin/program $OPTIONS

> [!NOTE]
> The example sets `<!>` to denote that it doesn't support `SIGHUP`.
> That way Finit will stop/start the service instead of sending SIGHUP
> at restart/reload events.
