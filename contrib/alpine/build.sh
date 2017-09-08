#!/bin/sh -e

if [ ! -f configure -a -f autogen.sh ]; then
   echo "Building from GIT, creating configure script ..."
   ./autogen.sh
fi

PKG_CONFIG_LIBDIR=/usr/local/lib/pkgconfig ./configure				\
		 --enable-rw-rootfs            --enable-progress		\
                 --enable-dbus-plugin          --enable-x11-common-plugin	\
		 --enable-alsa-utils-plugin    --enable-inetd-echo-plugin	\
		 --enable-inetd-chargen-plugin --enable-inetd-daytime-plugin	\
		 --enable-inetd-discard-plugin --enable-inetd-time-plugin	\
		 --with-heading="Alpine Linux 3.6" --with-hostname=alpine

make
