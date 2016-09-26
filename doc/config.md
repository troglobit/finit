Configuration
=============

Table of Contents
-----------------

* [/etc/finit.conf](#etcfinitconf)
* [/etc/finit.d](#etcfinitd)

/etc/finit.conf
---------------

Contrary to most other script based init alternatives (SysV [init][],
[upstart][], [systemd][], [OpenRC][], etc.)  Finit relies on its main
configuration file `/etc/finit.conf` and `/etc/finit.d/`.

The command line arguments given in `/etc/finit.conf` to each service
provide a default.  A plugin can be used to register a callback to a
service and then modify the behavior to suit the current runlevel and
system configuration.

For instance, before starting a heavily resource intensive service like
IPsec or OpenVPN, a callback can check if the outbound interface is up
and has an IP address, or just check if the service is disabled — much
like what a SysV init start script usually does.

Syntax:

* `check <DEV>`  
  Run fsck on a file system.  Only needed if not listed in `/etc/fstab`,
  Finit 3.0 and later check all file systems automatically at boot.

* `module <MODULE>`  
  Load a kernel module, with optional arguments

* `network <PATH>`  
  Script or program to bring up networking, with optional arguments

* `rlimit <hard|soft> RESOURCE <LIMIT|infinity>`  
  Modify the specified resource's hard or soft limit to be the
  specifed limit. `RESOURCE` is a lower-case string matching the
  constants in `setrlimit(2)` with the `RLIMIT_` prefix
  removed. E.g. to select `RLIMIT_CPU`, `RESOURCE` would be
  `cpu`. `LIMIT` is an integer whose unit depends on the resource
  being modified, see `setrlimit(2)` for more information. The special
  limit `infinity` means the there should be no limit on the resource,
  i.e. `RLIM_INFINITY` is passed to `setrlimit(2)`.

```shell
        # No process is allowed more than 8MB of address space
        rlimit hard as 8388608

        # Core dumps may be arbitrarily large
        rlimit soft core infinity
```

* `runlevel <N>`  
  N is the runlevel number 1-9, where 6 is reserved for reboot.  
  Default is 2.

* `run [LVLS] <COND> /path/to/cmd ARGS -- Optional description`  
  One-shot command to run in sequence when entering a runlevel, with
  optional arguments and description.  This command is guaranteed to be
  completed before running the next command.

* `task [LVLS] <COND> /path/to/cmd ARGS -- Optional description`  
  One-shot like 'run', but starts in parallel with the next command

* `service [LVLS] <COND> /path/to/daemon ARGS -- Optional description`  
  Service, or daemon, to be monitored and automatically restarted if it
  exits prematurely.  Please note that you often need to provide a
  `--foreground` or `--no-background` argument to most daemons to
  prevent them from forking off a sub-process in the background.

```shell
        service [2345] <svc/sbin/zebra> /sbin/ospfd -- OSPF daemon
```

  For a detailed description of conditions, and how to debug them,
  see the [Finit Conditions](conditions.md) document.

* `inetd service/proto[@iflist] <wait|nowait> [LVLS] /path/to/daemon args`  
  Launch a daemon when a client initiates a connection on an Internet
  port.  Available services are listed in the UNIX `/etc/services` file.
  Finit can filter access to from a list of interfaces, `@iflist`, per
  inetd service as well as listen to custom ports.

```shell
        inetd ftp/tcp	nowait	@root	/usr/sbin/uftpd -i -f
        inetd tftp/udp	wait	@root	/usr/sbin/uftpd -i -t
```

  The following example listens to port 2323 for telnet connections and
  only allows clients connecting from `eth0`:

```shell
        inetd 2323/tcp@eth0 nowait [2345] /sbin/telnetd -i -F
```

  The interface list, `@iflist`, is of the format `@iface,!iface,iface`,
  where a single `!` means to deny access.  Notice how interfaces are
  comma separated with no spaces.

  The `inetd` directive can also have ` -- Optional Description`, only
  Finit does not output this text on the console when launching inetd
  services.  Instead this text is sent to syslog and also shown by the
  `initctl` tool.  More on inetd below.

* `runparts <DIR>`  
  Call [run-parts(8)][] on `DIR` to run start scripts.  All executable
  files, or scripts, in the directory are called, in alphabetic order.

  It can be beneficial to use `S01name`, `S02othername`, etc. if there
  is a dependency order between the scripts.  Symlinks to existing
  daemons can talso be used, but make sure they daemonize by default.

  As the optional `/etc/rc.local` shell script, make sure that all your
  services and programs either terminate or start in the background or
  you will block Finit.

* `include <CONF>`  
  Include another configuration file.  Absolute path required.

* `tty [LVLS] <DEV> [BAUD] [TERM]`  
  Start a getty on the given TTY device DEV, in the given runlevels.  If
  no tty setting is given in `finit.conf` no login is possible.  Use the
  `service` stanza to start a stand-alone shell for really bare bones
  systems.

  Finit comes with a lightweight built-in getty, which after it has
  opened a TTY and the user has input a username, calls `/bin/login`
  like any other getty.

* `console <DEV>`  
  Use this, along with a matching `tty DEV` line, to mark this TTY as a
  special "console" port with with `prctl()`.  Useful in use-cases when
  all logins except the console must be stopped, e.g. when performing a
  flash upgrade on an embedded system.

When running <kbd>make install</kbd> no default `/etc/finit.conf` will
be installed since system requirements differ too much.  Try out the
Debian 6.0 example `/usr/share/doc/finit/finit.conf` configuration that
is capable of service monitoring SSH, sysklogd, gdm and a console getty!

Every `run`, `task`, `service`, or `inetd` can also list the privileges
the `/path/to/cmd` should be executed with.  Simply prefix the path with
`[@USR[:GRP]]` like this:

```shell
    run [2345] @joe:users /usr/bin/logger "Hello world"
```

For multiple instances of the same command, e.g. a DHCP client or
multiple web servers, add `:ID` somewhere between the `run`, `task`,
`service` keyword and the command, like this:

```shell
    service :1 [2345] /sbin/httpd -f -h /http -p 80   -- Web server
    service :2 [2345] /sbin/httpd -f -h /http -p 8080 -- Old web server
```

Without the `:ID` to the service the latter will overwrite the former
and only the old web server would be started and supervised.

The `run`, `task`, `service`, or `inetd` stanzas also allow the keyword
`log` to redirect `stderr` and `stdout` of the application to syslog,
using `logger`.

Worth noting is that conditions is allowed for all these stanzas.  For a
detailed description, see the [Conditions](conditions.md) document.


/etc/finit.d
------------

Finit supports changes to the overall system configuration at runtime.
For this purpose the (configurable) directory `/etc/finit.d` is used.
Here you can put configuration file snippets, one per service if you
like, which are all sourced automatically by finit at boot when loading
the static configuration from `/etc/finit.conf`.  This is the default
behavior, so no include directives are necessary.

To add a new service, simply drop a `.conf` file in `/etc/finit.d` and
run `initctl reload`.  (It is also possible to `SIGHUP` to PID 1, or
call `finit q`, but that has been deprecated with the `initctl` tool).
Any service read from this directory is flagged as a dynamic service, so
changes to or removal of `/etc/finit.d/*.conf` files, is detected.

On `initctl reload` the following is checked for all services:

- If a service's `.conf` file has been removed, or its conditions are no
  longer satisifed, the service is stopped.
- If the file is modified, or a service it depends on has been reloaded,
  the service is reloaded (stopped and started).
- If a new service is added it is automatically started — respecting
  runlevels and return values from any callbacks.

For more info on the different states of a service, see the separate
document [Finit Services](service.md).

The `/etc/finit.d` directory was previously the default Finit `runparts`
directory.  Finit no longer has a default `runparts`, so make sure to
update your setup, or the finit configuration, accordingly.

**Note:** Configurations read from `/etc/finit.d` are read *after*
  initial bootstrap, runlevel S(1).  Hence, bootstrap services *must* be
  in `/etc/finit.conf`!

[init]:         http://en.wikipedia.org/wiki/Init
[upstart]:      http://upstart.ubuntu.com/
[systemd]:      http://www.freedesktop.org/wiki/Software/systemd/
[openrc]:       http://www.gentoo.org/proj/en/base/openrc/
[run-parts(8)]: http://manpages.debian.org/cgi-bin/man.cgi?query=run-parts

<!--
  -- Local Variables:
  -- mode: markdown
  -- End:
  -->
