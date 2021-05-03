#!/bin/sh

if [ "x`id -u`" != "x0" ]; then
    echo
    echo "=> Unfortunately this script must run as root (sudo) <="
    echo
    exit 1
fi

# Adjust base directory
grep -q Debian os-release 2>/dev/null
if [ $? -eq 0 ]; then
    cd ../..
elif [ ! -e configure.ac ]; then
    echo "*** Please run this script from the Finit base directory."
    exit 1
fi

echo
echo "*** Install Finit on Debian GNU/Linux"
echo "========================================================================"
echo "/sbin/finit           - PID 1"
echo "/lib/finit/plugins/*  - All enabled Finit plugins"
echo "/etc/finit.conf       - Finit configuration file"
echo "/etc/finit.d/         - Finit services"
echo "/etc/grub.d/10_linux  - Add Finit to Grub's SUPPORTED_INITS"
echo
read -p "Do you want to continue (y/N)? " yorn
echo

if [ "x$yorn" = "xy" -o "x$yorn" = "xY" ]; then
    echo "Installing Finit files ..."
    make install
    cd /usr/share/doc/finit/contrib/debian
    for file in finit.conf; do
	install -vbD $file /etc/$file
    done
    cp -va finit.d /etc/

    echo "*** Setting up a GRUB boot entry ..."
    fn=/etc/grub.d/10_linux
    if [ -e $fn ]; then
	if `grep SUPPORTED_INITS $fn |head -1 |grep -q finit`; then
	    echo "Already installed, done."
	else
	    echo "Adding Finit to list of SUPPORTED_INITS ..."
	    sed -i 's/SUPPORTED_INITS="[^"]*/& finit:\/sbin\/finit/' $fn
	    update-grub
	fi
    else
	echo "Cannot find $fn, you'll have to set up your bootloader on your own."
    fi

    echo
    echo "*** Done"
    echo
else
    echo
    echo "*** Aborting install."
    echo
fi
