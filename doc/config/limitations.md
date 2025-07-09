Limitations
===========

As of Finit v4 there are no limitations to where `.conf` settings can be
placed.  Except for the system/global `rlimit` and `cgroup` top-level
group declarations, which can only be set from `/etc/finit.conf`, since
it is the first `.conf` file Finit reads.

Originally, `/etc/finit.conf` was the only way to set up a Finit system.
Today it is mainly used for bootstrap settings like system hostname,
early module loading for watchdogd, network bringup and system shutdown.
These can now also be set in any `.conf` file in `/etc/finit.d`.

There is, however, nothing preventing you from having all configuration
settings in `/etc/finit.conf`.

> [!IMPORTANT]
> The default `rcsd`, i.e., `/etc/finit.d`, was previously the Finit
> [runparts](runparts.md) directory.  Finit >=v4.0 no longer has a
> default `runparts` directory, make sure to update your setup, or the
> finit configuration, accordingly.
