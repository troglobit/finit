#!/bin/sh

if [ "x`id -u`" != "x0" ]; then
    echo
    echo "*** This script must run as root"
    echo
    exit 1
fi

read -p "Do you really want to uninstall Finit (y/N)? " yorn

if [ "x$yorn" = "xy" -o "x$yorn" = "xY" ]; then
    echo
    echo "*** Removing all Finit files ..."
    rm -rf /usr/share/doc/finit
    rm -rf /etc/finit.conf
    rm -rf /etc/finit.d
    rm -rf /lib/finit
    if [ -e /etc/rc.local ]; then
	echo "*** Skipping /etc/rc.local, not sure if we installed it ..."
    fi

    echo "*** Restoring BusyBox as /sbin/init ..."
    cd /sbin
    rm init
    ln -s /bin/busybox init

    rm halt shutdown reboot suspend
    ln -s /bin/busybox halt
    ln -s /bin/busybox shutdown
    ln -s /bin/busybox reboot
    ln -s /bin/busybox suspend
    echo "*** Done"
    echo
else
    echo
    echo "*** Aborting uninstall."
    echo
fi
