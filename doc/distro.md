Distro Recommendations
======================

By default Finit uses the following directories for configuration files:

```
    /etc/
      |-- finit.d/              -- Regular (enabled) services
      |    |-- lighttpd.conf
      |     `- *.conf
      |-- finit.conf            -- Bootstrap tasks and services
      :
```

To enable a service one simply drops a small configuration file in the
`/etc/finit.d/` directory.  This practice works with systems that
keep disabled services elsewhere, or generates them as needed from some
other tool.

Distributions, however, may want a clearer separation of enabled and
available (installed but not enabled) services.  They may even want to
customize the directories used, for brand labeling or uniformity.

To that end Finit allows for a sub-directory `/etc/finit.d/available/`
where installed but disabled services can reside.  Adding a symlink to a
configuration in this sub-directory enables the service, but it will not
be started.

To change the default configuration directory and configuration file
names the Finit `configure` script offers the following two options at
build time:

```sh
./configure --with-rcsd=/etc/init.d --with-config=/etc/init.d/init.conf
```

The resulting directory structure is depicted below.  Please notice how
`/etc/finit.conf` now resides in the same sub-directory as a non-symlink
`/etc/init.d/init.conf`:

```
    /etc/
      |-- init.d/
      |    |-- available/      -- Regular (disabled) services
	  |    |    |-- httpd.conf
	  |    |    |-- ntpd.conf
	  |    |    `-- sshd.conf
	  |    |-- sshd.conf       -- Symlink to available/sshd.conf
      |     `- init.conf       -- Bootstrap tasks and services
      :
```

To facilitate the task of managing configurations, be it a service,
task, run, or other stanza, the `initctl` tool has a a few built-in
commands:

```
   list              List all .conf in /etc/finit.d/
   enable   <CONF>   Enable   .conf in /etc/finit.d/available/
   disable  <CONF>   Disable  .conf in /etc/finit.d/[enabled/]
   reload            Reload  *.conf in /etc/finit.d/ (activates changes)
```

To enable a service like `sshd.conf`, above, use

    initctl enable sshd

The `.conf` suffix is not needed, `initctl` adds it implicitly if it is
missing.  The `disable` command works in a similar fashion.

Note, however, that `initctl` only operates on symlinks and it always
requires the `available/` sub-directory.  Any non-symlink in the parent
directory, here `/etc/init.d/`, will be considered a system override by
`initctl`.

