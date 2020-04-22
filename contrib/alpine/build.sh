#!/bin/sh -e

echo
echo "*** Configuring Finit for Alpine Linux"

if [ ! -e configure ]; then
    echo "    The configure script is missing, maybe you're using a version from GIT?"
    echo "    Attempting to run the autogen.sh script, you will need these tools:"
    echo "    autoconf, automake, libtool, pkg-config ..."
    echo
   ./autogen.sh
fi

# The plugins are optional, but you may need D-Bus and X11 if you want
# to run X-Window, the other configure flags are however required.
PKG_CONFIG_LIBDIR=/usr/lib/pkgconfig:/usr/local/lib/pkgconfig ./configure	\
		                               --enable-progress		\
                 --enable-dbus-plugin          --enable-x11-common-plugin	\
		 --enable-alsa-utils-plugin    --enable-inetd-echo-plugin	\
		 --enable-inetd-chargen-plugin --enable-inetd-daytime-plugin	\
		 --enable-inetd-discard-plugin --enable-inetd-time-plugin	\
		 --with-heading="Alpine Linux 3.6" --with-hostname=alpine

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

