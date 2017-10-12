Internet Super Server
=====================

Inetd
-----

A built-in *Internet Super Server* support was added in Finit v1.12 and
v1.13, along with an internal `time` inetd service, RFC 868 (rdate).
The latter is supplied as a plugin to illustrate how simple it is to
extend finit with more internal inetd services.  Today more built-in
services are available.

> Please note, not all UNIX daemons are prepared to run as inetd services.
> In the example below `sshd` also need the command line argument `-i`.

The inetd support in finit is quite advanced.  Not only does it launch
services on demand, it can do so on custom ports and also filter inbound
traffic using a poor man's [TCP wrappers][].  The syntax is very similar
to the traditional `/etc/inetd.conf`, yet keeping with the style of
Finit:

```shell
    # Launch SSH on demand, in runlevels 2-5 as root
    inetd ssh/tcp            nowait [2345] @root:root /usr/sbin/sshd -i
```

A more advanced example is listed below, please note the *incompatible
syntax change* that was made between Finit v1.12 and v1.13 to support
deny filters:

```shell
    # Start sshd if inbound connection on eth0, port 222, or
    # inbound on eth1, port 22.  Ignore on other interfaces.
    inetd 222/tcp@eth0       nowait [2345] /usr/sbin/sshd -i
    inetd ssh/tcp@eth1,eth1  nowait [2345] /usr/sbin/sshd -i
```
	
If `eth0` is your Internet interface you may want to avoid using the
default port.  To run ssh on port 222, and all others on port 22:
    
```shell
    inetd 222/tcp@eth0       nowait [2345] /usr/sbin/sshd -i
    inetd ssh/tcp@*,!eth0    nowait [2345] /usr/sbin/sshd -i
```

Compared to Finit v1.12 you must *explicitly deny* access from `eth0`!

To protect against looping attacks, the inetd server will refuse UDP
service if the reply port corresponds to any internal service.  Similar
to how the FreeBSD inetd operates.


**Internal Services**

Like the original `inetd`, Finit has a few standard services built-in.
They are realized as plugins to provide a simple means of testing the
inetd functionality stand-alone.  But this also provides both a useful
network testing/availability, as well as a rudimentary time server for
`rdate` clients.

- echo
- chargen
- daytime
- discard
- time

For security reasons they are all disabled by default and have to be
enabled with both the `configure` script and a special `inetd` stanza in
the `finit.conf` or `finit.d/*.conf` like this:

```shell
    inetd echo/udp           wait [2345] internal
    inetd echo/tcp         nowait [2345] internal
    inetd chargen/udp        wait [2345] internal
    inetd chargen/tcp      nowait [2345] internal
    inetd daytime/udp        wait [2345] internal
    inetd daytime/tcp      nowait [2345] internal
    inetd discard/udp        wait [2345] internal
    inetd discard/tcp      nowait [2345] internal
    inetd time/udp           wait [2345] internal
    inetd time/tcp         nowait [2345] internal
```

Then call `rdate` from a remote machine (or use localhost):

```shell
    rdate -p  <IP>
    rdate -up <IP>
```

Or `echoping` to reach the echo service:

```shell
    echoping -v  <IP>
    echoping -uv <IP>
```

Or `echoping -d` to reach the discard service:

```shell
    echoping -dv  <IP>
    echoping -duv <IP>
```

Or `echoping -c` to reach the chargen service:

```shell
    echoping -cv  <IP>
    echoping -cuv <IP>
```

If you use `time/udp` you must use the standard rdate implementation and
then call it with `rdate -up` to connect using UDP.  Without the `-p`
argument rdate will try to set the system clock.  Please note that rdate
has been deprecated by the NTP protocol and this plugin should only be
used for testing or environments where NTP for some reason is blocked.
Also, remember the UNIX year 2038 bug, or in the case of RFC 868 (and
some NTP implementations), year 2036!

**Note:** There is currently no verification that the same port is used
  more than once.  So a standard `inetd http/tcp` service will clash
  with an ssh entry for the same port `inetd 80/tcp` …


[TCP Wrappers]:     https://en.wikipedia.org/wiki/TCP_Wrapper
