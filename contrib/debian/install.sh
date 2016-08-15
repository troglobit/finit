#!/bin/sh

SYMLINKS="start.d/*"
FILES="finit.conf"

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
elif [ ! -e finit.c ]; then
    echo "Please run this script from the Finit base directory."
    exit 1
fi

echo "Install Finit on Debian 8 (jessie)"
echo "========================================================================"
echo "/sbin/finit           - PID 1"
echo "/lib/finit/plugins/*  - All enabled Finit plugins"
echo "/etc/finit.conf       - Instead of units or /etc/init.d Finit can have"
echo "                        one config for your system.  Or you can split"
echo "                        it in service(s) per file in /etc/finit.d/"
echo "/etc/start.d/         - Simple Debian setup scripts run with run-parts(8)"
echo "/etc/grub.d/40_custom - Add menu entry to the Grub boot loader"
echo
read -p "Do you want to continue (y/N)? " yorn
echo

if [ "x$yorn" = "xy" -o "x$yorn" = "xY" ]; then
    echo "Installing Finit files ..."
    make install
    cd `dirname $0`
    mkdir -p /etc/start.d
    for file in $FILES; do
	install -vbD $file /etc/$file
    done
    for link in $SYMLINKS; do
	cp -va $link /etc/$link
    done
    echo "Setting up a GRUB boot entry ..."
    MENU=`grep gnu-linux /boot/grub/grub.cfg |head -1 |sed "s/menuentry '\([^']*\).*/\1/"`
    TYPE=`grep gnu-linux /boot/grub/grub.cfg |head -1 |sed "s/.*' --class \([a-z]*\).*/\1/"`
    BOOT=`grep set=root /boot/grub/grub.cfg |head -1 |sed 's/.* \([a-f0-9]*-[a-f0-9]*-[a-f0-9]*-[a-f0-9]*-[a-f0-9]*\).*/\1/'`
    ROOT=`grep root=UUID /boot/grub/grub.cfg |head -1 |sed 's/.*UUID=\([a-f0-9]*-[a-f0-9]*-[a-f0-9]*-[a-f0-9]*-[a-f0-9]*\).*/\1/'`
    cat grub.d/40_custom | sed "s,\$BOOT,$BOOT,;s,\$ROOT,$ROOT,;s,\$TYPE,$TYPE,;s,\$MENU,$MENU," >/etc/grub.d/40_custom
    update-grub
else
    echo "Aborting install."
fi
