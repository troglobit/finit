#!/bin/sh
#
# This script configures Finit for Debian, which needs a few more plugins
# and tweaks than an embedded system usually does.
#
# Usage:
#         user@jessie:~/finit$ contrib/debian/build.sh
#

echo
echo "*** Configuring Finit for Debian"

if [ ! -e configure ]; then
    echo "    The configure script is missing, maybe you're using a GIT checkout?"
    echo "    Attempting to run the autogen.sh script, you need these tools:"
    echo "    autoconf, automake, libtool, pkg-config ..."
    echo
    ./autogen.sh
fi
echo

./configure --enable-rw-rootfs --enable-dbus-plugin --enable-x11-common-plugin     \
	--enable-alsa-utils-plugin --with-random-seed=/var/lib/urandom/random-seed \
	--with-heading="Debian GNU/Linux 8.5" --with-hostname=jessie               \
	--enable-inetd-echo-plugin --enable-inetd-chargen-plugin                   \
	--enable-inetd-daytime-plugin --enable-inetd-discard-plugin                \
	--enable-inetd-time-plugin

if [ $? -ne 0 ]; then
    echo
    echo "*** Configure script failed, have you installed libuEV and libite?"
    echo
    exit 1
fi

echo
echo "*** Building ..."
echo
make

if [ $? -ne 0 ]; then
    echo
    echo "*** The build failed for some reason"
    echo
    exit 1
fi

echo
echo "*** Done"
echo

read -p "*** Run (sudo) install script (y/N)? " yorn
if [ "x$yorn" = "xy" -o "x$yorn" = "xY" ]; then
    sudo contrib/debian/install.sh
else
    echo
    echo "*** Use 'sudo contrib/debian/install.sh' later to install Finit"
    echo
fi


    
