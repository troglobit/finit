Commands & Status
=================

Finit also implements a modern API to query status, and start/stop
services, called `initctl`.  Unlike `telinit` the `initctl` tool does
not return until the given command has fully completed.

```
Usage: initctl [OPTIONS] [COMMAND]

Options:
  -b, --batch               Batch mode, no screen size probing
  -c, --create              Create missing paths (and files) as needed
  -f, --force               Ignore missing files and arguments, never prompt
  -h, --help                This help text
  -j, --json                JSON output in 'status' and 'cond' commands
  -1, --once                Only one lap in commands like 'top'
  -p, --plain               Use plain table headings, no ctrl chars
  -q, --quiet               Silent, only return status of command
  -t, --no-heading          Skip table headings
  -v, --verbose             Verbose output
  -V, --version             Show program version

Commands:
  debug                     Toggle Finit (daemon) debug
  help                      This help text
  version                   Show program version

  ls | list                 List all .conf in /etc/finit.d
  create   <CONF>           Create   .conf in /etc/finit.d/available
  delete   <CONF>           Delete   .conf in /etc/finit.d/available
  show     <CONF>           Show     .conf in /etc/finit.d/available
  edit     <CONF>           Edit     .conf in /etc/finit.d/available
  touch    <CONF>           Change   .conf in /etc/finit.d/available
  enable   <CONF>           Enable   .conf in /etc/finit.d/available
  disable  <CONF>           Disable  .conf in /etc/finit.d/enabled
  reload                    Reload  *.conf in /etc/finit.d (activate changes)

  cond     set   <COND>     Set (assert) user-defined conditions     +usr/COND
  cond     get   <COND>     Get status of user-defined condition, see $? and -v
  cond     clear <COND>     Clear (deassert) user-defined conditions -usr/COND
  cond     status           Show condition status, default cond command
  cond     dump  [TYPE]     Dump all, or a type of, conditions and their status

  log      [NAME]           Show ten last Finit, or NAME, messages from syslog
  start    <NAME>[:ID]      Start service by name, with optional ID
  stop     <NAME>[:ID]      Stop/Pause a running service by name
  reload   <NAME>[:ID]      Reload service as if .conf changed (SIGHUP or restart)
                            This allows restart of run/tasks that have already run
                            Note: Finit .conf file(s) are *not* reloaded!
  restart  <NAME>[:ID]      Restart (stop/start) service by name
  signal   <NAME>[:ID] <S>  Send signal S to service by name, with optional ID
  ident    [NAME]           Show matching identities for NAME, or all
  status   <NAME>[:ID]      Show service status, by name
  status                    Show status of services, default command

  cgroup                    List cgroup config overview
  ps                        List processes based on cgroups
  top                       Show top-like listing based on cgroups

  plugins                   List installed plugins

  runlevel [0-9]            Show or set runlevel: 0 halt, 6 reboot
  reboot                    Reboot system
  halt                      Halt system
  poweroff                  Halt and power off system
  suspend                   Suspend system

  utmp     show             Raw dump of UTMP/WTMP db
```

For services *not* supporting `SIGHUP` the `<!>` notation in the .conf
file must be used to tell Finit to stop and start it on `reload` and
`runlevel` changes.  If `<>` holds more [conditions](conditions.md),
these will also affect how a service is maintained.

> [!NOTE]
> Even though it is possible to start services not belonging in the
> current runlevel these services will not be respawned automatically by
> Finit if they exit (crash).  Hence, if the runlevel is 2, the below
> Dropbear SSH service will not be restarted if it is killed or exits.

The `status` command is the default, it displays a quick overview of all
monitored run/task/services.  Here we call `initctl -p`, suitable for
scripting and documentation:

```
alpine:~# initctl -p
PID   IDENT     STATUS   RUNLEVELS     DESCRIPTION
======================================================================
1506  acpid     running  [---2345----] ACPI daemon
1509  crond     running  [---2345----] Cron daemon
1489  dropbear  running  [---2345----] Dropbear SSH daemon
1511  klogd     running  [S-12345----] Kernel log daemon
1512  ntpd      running  [---2345----] NTP daemon
1473  syslogd   running  [S-12345----] Syslog daemon

alpine:~# initctl -pv
PID   IDENT     STATUS   RUNLEVELS     COMMAND
======================================================================
1506  acpid     running  [---2345----] acpid -f
1509  crond     running  [---2345----] crond -f -S $CRON_OPTS
1489  dropbear  running  [---2345----] dropbear -R -F $DROPBEAR_OPTS
1511  klogd     running  [S-12345----] klogd -n $KLOGD_OPTS
1512  ntpd      running  [---2345----] ntpd -n $NTPD_OPTS
1473  syslogd   running  [S-12345----] syslogd -n
```

The environment variables to each of the services above are read from,
in the case of Alpine Linux, `/etc/conf.d/`.  Other distributions may
have other directories, e.g., Debian use `/etc/default/`.

The `status` command takes an optional `NAME:ID` argument.  Here we
check the status of `dropbear`, which only has one instance in this
system:

```
alpine:~# initctl -p status dropbear
     Status : running
   Identity : dropbear
Description : Dropbear SSH daemon
     Origin : /etc/finit.d/enabled/dropbear.conf
Environment : -/etc/conf.d/dropbear
Condition(s):
    Command : dropbear -R -F $DROPBEAR_OPTS
   PID file : !/run/dropbear.pid
        PID : 1485
       User : root
      Group : root
     Uptime : 2 hour 46 min 56 sec
  Runlevels : [---2345----]
     Memory : 1.2M
     CGroup : /system/dropbear cpu 0 [100, max] mem [--.--, max]
              |- 1485 dropbear -R -F
              |- 2634 dropbear -R -F
              |- 2635 ash
              `- 2652 initctl -p status dropbear

Apr  8 12:19:49 alpine authpriv.info dropbear[1485]: Not backgrounding
Apr  8 12:37:45 alpine authpriv.info dropbear[2300]: Child connection from 192.168.121.1:47834
Apr  8 12:37:46 alpine authpriv.notice dropbear[2300]: Password auth succeeded for 'root' from 192.168.121.1:47834
Apr  8 12:37:46 alpine authpriv.info dropbear[2300]: Exit (root) from <192.168.121.1:47834>: Disconnect received
Apr  8 15:02:11 alpine authpriv.info dropbear[2634]: Child connection from 192.168.121.1:48576
Apr  8 15:02:12 alpine authpriv.notice dropbear[2634]: Password auth succeeded for 'root' from 192.168.121.1:48576
```
