This section provides an overview of Finit's configuration system. For
detailed information on specific topics, see the individual sections in
the navigation menu.


Configuration File Syntax
--------------------------

The file format is line based, empty lines and comments, lines starting
with `#`, are ignored.  A configuration directive starts with a keyword
followed by a space and the rest of the line is treated as the value.

As of Finit v4.4, configuration directives can be broken up in multiple
lines using the continuation character `\`, and trailing comments are
also allowed.  Example:

```aconf
# Escape \# chars if you want them literal in, e.g., descriptions
service name:sysklogd [S123456789]   \
    env:-/etc/default/sysklogd       \
    syslogd -F $SYSLOGD_ARGS         \
    -- System log daemon \# 1   # Comments allowed
```

The .conf files `/etc/finit.conf` and `/etc/finit.d/*` support many
directives.  Some are restricted, e.g., only available at bootstrap,
runlevel `S`.  Read on in [Files & Layout](files.md) for more on how
to structure your .conf files.

For details on restrictions, see [Limitations](limitations.md).
