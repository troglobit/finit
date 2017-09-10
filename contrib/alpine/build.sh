#!/bin/sh -e

if [ ! -f configure -a -f autogen.sh ]; then
   echo "Building from GIT, creating configure script ..."
   ./autogen.sh
fi

# The plugins are optional, but you may need D-Bus and X11 if you want
# to run X-Window, the other configure flags are however required.
PKG_CONFIG_LIBDIR=/usr/local/lib/pkgconfig ./configure				\
		 --enable-rw-rootfs            --enable-progress		\
                 --enable-dbus-plugin          --enable-x11-common-plugin	\
		 --enable-alsa-utils-plugin    --enable-inetd-echo-plugin	\
		 --enable-inetd-chargen-plugin --enable-inetd-daytime-plugin	\
		 --enable-inetd-discard-plugin --enable-inetd-time-plugin	\
		 --with-heading="Alpine Linux 3.6" --with-hostname=alpine

make
