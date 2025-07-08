Templating
==========

Finit comes with rudimentary support for templating, similar to that of
systemd.  Best illustrated with an example:

    $ initctl show avahi-autoipd@
    service :%i avahi-autoipd --syslog %i -- ZeroConf for %i

To enable ZeroConf for, e.g., `eth0`, use

    $ initctl enable avahi-autoipd@eth0.conf

The enabled symlink will be set up to `avahi-autoipd@.conf` and every
instance of `%i` will in the instantiated directive be replaced with
`eth0`.  Inspect the resulting instantiated template with `initctl show
avahi-autoipd:eth0` and check the status of a running instance with:

```
$ initctl status avahi-autoipd:eth0
     Status : running
   Identity : avahi-autoipd:eth0
Description : ZeroConf for eth0
     Origin : /etc/finit.d/enabled/avahi-autoipd@eth0.conf
Environment : -/etc/default/avahi-autoipd-eth0
    Command : avahi-autoipd $AVAHI_AUTOIPD_ARGS eth0
   PID file : /run/avahi-autoipd.eth0.pid
        PID : 4190
       User : root
      Group : root
     Uptime : 24 sec
   Restarts : 0 (0/10)
  Runlevels : [---2345-789]
     Memory : 20.0k
     CGroup : /system/avahi-autoipd@eth0 cpu 0 [100, max] mem [0, max]
              └─ 4190 avahi-autoipd: [eth0] bound 169.254.1.9  

Jul  8 11:51:42 infix-c0-ff-ee finit[1]: Starting avahi-autoipd:eth0[4190]
```
