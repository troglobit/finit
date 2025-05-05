#!/bin/sh
# Run test suite with the correct configure params

make distclean
./configure --prefix=/usr --exec-prefix= --sysconfdir=/etc --localstatedir=/var \
	    --enable-x11-common-plugin --with-watchdog --with-keventd \
	    CFLAGS='-fsanitize=address -ggdb'
make -j17 clean check
