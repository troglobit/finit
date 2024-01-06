#!/bin/sh

if [ "$(id -u)" != "0" ]; then
    printf "\n\e[1m*** This script must run as root\e[0m\n\n"
    exit 1
fi

# shellcheck disable=SC3037,SC3045,SC2154
yorn()
{
    printf "\e[1m> %s (y/N)?\e[0m " "$*"
    read -rn1 yorn
    echo

    if [ "$yorn" = "y" ] || [ "$yorn" = "Y" ]; then
	return 0
    fi

    return 1
}

# Adjust base directory
if grep -q contrib/alpine Makefile 2>/dev/null; then
    cd ../..
fi

if [ ! -e configure.ac ]; then
    printf "\n\e[1m*** Please run this script from the Finit base directory.\e[0m\n"
    exit 1
fi

printf "\n\e[1mInstall Finit on Alpine Linux\n"
printf "========================================================================\e[0m\n"
printf "/sbin/finit           - PID 1\n"
printf "/lib/finit/plugins/*  - All enabled Finit plugins\n"
printf "/etc/finit.conf       - Finit configuration file\n"
printf "/etc/finit.d/         - Finit services\n"

if yorn "Do you want to continue"; then
    printf "\n\e[1m*** Installing Finit files ...\e[0m\n"
    make install
    cd /usr/share/doc/finit/contrib/alpine || exit 1
    for file in finit.conf rc.local; do
	install -vbD $file /etc/$file
    done
    cp -va finit.d /etc/

    if yorn "Install Finit as the system default Init"; then
	printf "\e[1m*** Updating /sbin/init symlink --> finit ...\e[0m\n"
	cd /sbin || exit 1
	rm init
	ln -s finit init

	rm halt shutdown suspend
	ln -s reboot halt
	ln -s reboot shutdown
	ln -s reboot suspend
    fi
    printf "\n\e[1m*** Done\e[0m\n"
    echo
else
    printf "\n\e[1m*** Aborting install.\e[0m\n\n"
fi
