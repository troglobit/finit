keventd
=======

The kernel event daemon bundled with Finit is a simple uevent monitor
for `/sys/class/power_supply`.  It provides the `sys/pwr/ac` condition,
which can be useful to prevent power hungry services like anacron to run
when a laptop is only running on battery, for instance.

Since keventd is not an integral part of Finit yet it is not enabled by
default.  Enable it using `./configure --with-keventd`.  The bundled
`contrib/` build scripts for Debian, Alpine, and Void have this enabled.

This daemon is planned to be extended with monitoring of other uevents,
patches and ideas are welcome in the issue tracker.
