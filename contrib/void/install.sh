#!/bin/sh

if [ "x`id -u`" != "x0" ]; then
    echo
    echo "*** This script must run as root"
    echo
    exit 1
fi

# Adjust base directory
if [ -e xbps ]; then
    cd ../..
elif [ ! -e autogen.sh ]; then
    echo "*** Please run this script from the Finit base directory."
    exit 1
fi

echo
echo "*** Install Finit on Void Linux"
echo "========================================================================"
echo "/sbin/finit           - PID 1"
echo "/lib/finit/plugins/*  - All enabled Finit plugins"
echo "/etc/finit.conf       - Finit configuration file"
echo "/etc/finit.d/         - Finit services"
echo "/etc/grub.d/40_custom - Add menu entry to the Grub boot loader"
echo
read -p "Do you want to continue (y/N)? " yorn

if [ "x$yorn" = "xy" -o "x$yorn" = "xY" ]; then
    echo
    echo "*** Installing Finit files ..."
    make install
    cd /usr/share/doc/finit/contrib/void
    for file in finit.conf rc.local; do
	install -vbD $file /etc/$file
    done
    cp -va finit.d /etc/

    read -p "*** Install Finit as the system default Init (y/N)? " yorn
    if [ "x$yorn" = "xy" -o "x$yorn" = "xY" ]; then
	echo "*** Updating /sbin/init symlink --> finit ..."
	cd /sbin
	rm init
	ln -s finit init
    fi

    echo "*** Done"
    echo
else
    echo
    echo "*** Aborting install."
    echo
fi
