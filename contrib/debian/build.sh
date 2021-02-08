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
    echo "    The configure script is missing, maybe you're using a version from GIT?"
    echo "    Attempting to run the autogen.sh script, you will need these tools:"
    echo "    autoconf, automake, libtool, pkg-config ..."
    echo
    ./autogen.sh
fi
echo

# The plugins are optional, but you may need D-Bus and X11 if you want
# to run X-Window, the other configure flags are however required.
./configure                                                                     \
    --prefix=/usr                     --exec-prefix=                            \
    --sysconfdir=/etc                 --localstatedir=/var                      \
                                      --enable-progress                         \
    --enable-dbus-plugin              --enable-x11-common-plugin                \
    --enable-alsa-utils-plugin        --enable-inetd-echo-plugin                \
    --enable-inetd-chargen-plugin     --enable-inetd-daytime-plugin             \
    --enable-inetd-discard-plugin     --enable-inetd-time-plugin                \
    --with-random-seed=/var/lib/urandom/random-seed                             \
    --with-heading="Debian GNU/Linux" --with-hostname="stretch"

if [ $? -ne 0 ]; then
    echo
    echo "*** Configure script failed, have you installed libuEv and libite?"
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


    
