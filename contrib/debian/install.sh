#!/bin/sh

if test "x`id -u`" != "x0"; then
    echo
    echo "=> Unfortunately this script must run as root (sudo) <="
    echo
    exit 1
fi

# Adjust base directory
grep -q Debian os-release 2>/dev/null
if test $? -eq 0; then
    cd ../..
elif ! test -e autogen.sh; then
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
echo "/etc/grub.d/40_custom - Add menu entry to the Grub boot loader"
echo
read -p "Do you want to continue (y/N)? " yorn
echo

if test "x$yorn" = "xy" || test "x$yorn" = "xY"; then
    echo "Installing Finit files ..."
    make install
    cd `dirname $0`
    install -vbD finit.conf /etc/finit.conf
    cp -va finit.d /etc/

    echo "*** Setting up a GRUB boot entry ..."
    MENU=`grep gnu-linux /boot/grub/grub.cfg |head -1 |sed "s/menuentry '\([^']*\).*/\1/"`
    TYPE=`grep gnu-linux /boot/grub/grub.cfg |head -1 |sed "s/.*' --class \([a-z]*\).*/\1/"`
    BOOT=`grep set=root /boot/grub/grub.cfg |head -1 |sed 's/.* \([a-f0-9]*-[a-f0-9]*-[a-f0-9]*-[a-f0-9]*-[a-f0-9]*\).*/\1/'`
    ROOT=`grep root=UUID /boot/grub/grub.cfg |head -1 |sed 's/.*UUID=\([a-f0-9]*-[a-f0-9]*-[a-f0-9]*-[a-f0-9]*-[a-f0-9]*\).*/\1/'`
    cat grub.d/40_custom | sed "s,\$BOOT,$BOOT,;s,\$ROOT,$ROOT,;s,\$TYPE,$TYPE,;s,\$MENU,$MENU," >/etc/grub.d/40_custom
    update-grub

    echo
    echo "*** Done"
    echo
else
    echo
    echo "*** Aborting install."
    echo
fi
