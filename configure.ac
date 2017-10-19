AC_INIT(Finit, 3.0, https://github.com/troglobit/finit/issues, finit, http://troglobit.com/finit.html)
AM_INIT_AUTOMAKE([1.11 foreign no-dist-gzip dist-xz subdir-objects])
AM_SILENT_RULES([yes])

AC_CONFIG_SRCDIR([src/finit.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile plugins/Makefile src/Makefile])

# Older versions of autoconf (<2.58) do not have AC_CONFIG_MACRO_DIR()
#m4_include([m4/plugin.m4])
#m4_include([m4/expand.m4])
AC_CONFIG_MACRO_DIR([m4])

# Change default prefix, we're PID 1 after all ...
AC_PREFIX_DEFAULT([])

# Handle building plugins either as dynamically loadable, or built-in
LT_INIT([shared disable-static dlopen aix-soname=both disable-fast-install])

# Checks for programs.
AC_PROG_CC
AC_PROG_LN_S
AC_PROG_INSTALL

# Configuration.
AC_HEADER_STDC
AC_CHECK_HEADERS([fstaby.h termios.h])
AC_CHECK_FUNCS([strstr getopt getfsenty])

# Check for uint[8,16,32]_t
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Check for required libraries
PKG_CHECK_MODULES([uev],  [libuev >= 1.5.2])
PKG_CHECK_MODULES([lite], [libite >= 1.9.2])

# Check for configured Finit features
AC_ARG_ENABLE(emergency_shell,
	AS_HELP_STRING([--enable-emergency-shell], [Experimental emergency fallback at boot.
	                Use only for debugging very early boot problems with Finit]),,[
	enable_emergency_shell=no])

AC_ARG_ENABLE(fallback_shell,
        AS_HELP_STRING([--enable-fallback-shell], [Start a shell if no TTYs can start]),,[
	enable_fallback_shell=no])

AC_ARG_ENABLE(rw-rootfs,
        AS_HELP_STRING([--enable-rw-rootfs], [Remount / R/W at boot, usually not for embedded]),,[
	enable_rw_rootfs=no])

AC_ARG_ENABLE(progress,
        AS_HELP_STRING([--enable-progress], [Pre Finit v3 bootup progress and heading]),,[
	enable_progress=no])

AC_ARG_ENABLE(watchdog,
        AS_HELP_STRING([--enable-watchdog], [Enable built-in watchdog, uses /dev/watchdog]),,[
	enable_watchdog=no])

AC_ARG_ENABLE(inetd,
        AS_HELP_STRING([--disable-inetd], [Disable built-in inetd super server, default enabled]),,[
	enable_inetd=yes])

AC_ARG_ENABLE(logrotate,
        AS_HELP_STRING([--disable-logrotate], [Disable built-in rotation of /var/log/wtmp, default enabled]),,[
	enable_logrotate=yes])

# Check for extra plugins to enable
enable_all_plugins=auto
AC_PLUGIN([alsa-utils],    [no],  [Save and restore ALSA sound settings using alsactl])
AC_PLUGIN([dbus],          [no],  [Setup and start system message bus, D-Bus])
AC_PLUGIN([inetd-echo],    [no],  [Inetd plugin: echo server, RFC862])
AC_PLUGIN([inetd-chargen], [no],  [Inetd plugin: character generator, RFC864])
AC_PLUGIN([inetd-daytime], [no],  [Inetd plugin: daytime server, RFC867])
AC_PLUGIN([inetd-discard], [no],  [Inetd plugin: discard server, RFC863])
AC_PLUGIN([inetd-time],    [no],  [Inetd plugin: time (rdate) server, RFC868])
AC_PLUGIN([lost],          [no],  [Example HOOK_SVC_LOST plugin, very noisy as-is!])
AC_PLUGIN([resolvconf],    [no],  [Setup necessary files for resolvconf])
AC_PLUGIN([x11-common],    [no],  [Console setup (for X)])
AC_PLUGIN([netlink],       [yes], [Basic netlink plugin for IFUP/IFDN and GW events. Can be replaced with externally built plugin that links with libnl or similar.])

# Check for extra arguments or packages
AC_ARG_WITH(heading,
        AS_HELP_STRING([--with-heading=GREET], [Display GREET instead of Finit version at boot]),
	[heading=$withval], [heading="$PACKAGE_NAME v$PACKAGE_VERSION"])

AC_ARG_WITH(fifo,
        AS_HELP_STRING([--with-fifo=FIFO], [FIFO for init/telinit clients, default /var/run/initctl]),
	[fifo=$withval], [fifo=$localstatedir/run/initctl])

AC_ARG_WITH(plugindir,
        AS_HELP_STRING([--with-plugindir=DIR], [Finit plugin directory, default /lib/finit/plugins]),
	[pkglibdir=$withval], [pkglibdir=$libdir/finit/plugins])
AC_SUBST([pkglibdir])

AC_ARG_WITH(config,
        AS_HELP_STRING([--with-config=CONFIG], [Finit bootstrap config file, default /etc/finit.conf]),
	[conf=$withval], [conf=$sysconfdir/finit.conf])

AC_ARG_WITH(rcsd,
        AS_HELP_STRING([--with-rcsd=DIR], [Finit rcS.d/ directory, default /etc/finit.d]),
	[rcsd=$withval], [rcsd=$sysconfdir/finit.d])

AC_ARG_WITH(rc-local,
        AS_HELP_STRING([--with-rc-local=FILE], [SysV init /etc/rc.local file, default /etc/rc.local]),
	[rclocal=$withval], [rclocal=$sysconfdir/rc.local])

AC_ARG_WITH(console,
        AS_HELP_STRING([--with-console=DEV], [Console to use, default: /dev/console]),
	[console=$withval], [console=/dev/console])

AC_ARG_WITH(hostname,
        AS_HELP_STRING([--with-hostname=NAME], [If /etc/hostname is missing, default: noname]),
	[hostname=$withval], [hostname=noname])

AC_ARG_WITH(runlevel,
        AS_HELP_STRING([--with-runlevel=N], [Runlevel to switch to after bootstrap, default: 2]),
	[runlevel=$withval], [runlevel=2])

AC_ARG_WITH(random-seed,
        AS_HELP_STRING([--with-random-seed=FILE], [Save a random seed for /dev/urandom across reboots, default /var/lib/misc/random-seed]),
	[random_seed=$withval], [random_seed=/var/lib/misc/random-seed])


### Enable features ###########################################################################

# Create config.h from selected features and fallback defautls
AS_IF([test "x$enable_emergency_shell" = "xyes"], [
        AC_DEFINE(EMERGENCY_SHELL,  1, [Fork off init and let PID 1 wait and start /bin/sh if init crashes.])])

AS_IF([test "x$enable_fallback_shell" = "xyes"], [
        AC_DEFINE(FALLBACK_SHELL,  1, [Start /bin/sh on console if no TTYs.])])

AS_IF([test "x$enable_rw_rootfs" = "xyes"], [
        AC_DEFINE(REMOUNT_ROOTFS,  1, [Remount / as rw at boot])])

AS_IF([test "x$enable_progress" = "xyes"], [
        AC_DEFINE(SILENT_MODE,  0, [Pre Finit v3 bootup messages and heading])
	], [
	AC_DEFINE(SILENT_MODE,  1, [Completely silent until login prompt])])

AS_IF([test "x$enable_watchdog" = "xyes"], [
        AC_DEFINE(BUILTIN_WATCHDOG,  1, [Enable built-in watchdog, kicks on /dev/watchdog])])

### Disable features ###########################################################################
AS_IF([test "x$enable_inetd" != "xno"], [
	enable_inetd="yes"
	AC_DEFINE(INETD_ENABLED,  1, [Enable built-in inetd])])

AS_IF([test "x$enable_logrotate" != "xno"], [
	AC_DEFINE(LOGROTATE_ENABLED, 1, [Enable built-in rotation of /var/log/wtmp et al.])])

### With features ##############################################################################
AS_IF([test "x$with_fifo" != "xno"], [
	AS_IF([test "x$fifo" = "xyes"], [
		    fifo='$localstatedir/run/initctl'])])
AC_EXPAND_DIR(fifo_path, "$fifo")
AC_DEFINE_UNQUOTED(FINIT_FIFO, "$fifo_path", [FIFO for old style init/telinit client programs])

AS_IF([test "x$with_config" != "xno"], [
	AS_IF([test "x$config" = "xyes"], [
		conf=$sysconfdir/finit.conf])])
AC_EXPAND_DIR(conf_path, "$conf")
AC_DEFINE_UNQUOTED(FINIT_CONF, "$conf_path", [Main Finit configuration file, default /etc/finit.conf])

AS_IF([test "x$with_rcsd" != "xno"], [
	AS_IF([test "x$rcsd" = "xyes"], [
		rcsd=$sysconfdir/finit.d])])
AC_EXPAND_DIR(rcsd_path, "$rcsd")
AC_DEFINE_UNQUOTED(FINIT_RCSD, "$rcsd_path", [Finit rcS.d/, default /etc/finit.d])

AS_IF([test "x$with_rc_local" != "xno"], [
	AS_IF([test "x$rclocal" = "xyes"], [
		rclocal=$sysconfdir/rc.local])])
AC_EXPAND_DIR(rclocal_path, "$rclocal")
AC_DEFINE_UNQUOTED(FINIT_RC_LOCAL, "$rclocal_path", [Compat SysV /etc/rc.local file to run last at boot.])

AS_IF([test "x$with_heading" != "xno"], [
	AS_IF([test "x$heading" = "xyes"], [
		heading="$PACKAGE_NAME v$PACKAGE_VERSION"])])
AC_DEFINE_UNQUOTED(INIT_HEADING, "$heading", [Displayed by Finit at boot])

AS_IF([test "x$with_console" != "xno"], [
	AS_IF([test "x$console" = "xyes"], [
		console=/dev/console])])
AC_DEFINE_UNQUOTED(CONSOLE, "$console", [Default console to use])

AS_IF([test "x$with_hostname" != "xno"], [
	AS_IF([test "x$hostname" = "xyes"], [
		hostname=noname])])
AC_DEFINE_UNQUOTED(DEFHOST, "$hostname", [For /etc/hostname])

AS_IF([test "x$with_runlevel" != "xno"], [
	AS_IF([test "x$runlevel" = "xyes"], [
		AC_DEFINE_UNQUOTED(RUNLEVEL, 2, [Default runlevel to start after S])])
	AC_DEFINE_UNQUOTED(RUNLEVEL, $runlevel, [Runlevel to start after S])])

AS_IF([test "x$with_random_seed" != "xno"], [
	AS_IF([test "x$random_seed" = "xyes"], [
		random_seed=/var/lib/misc/random-seed])
	AC_EXPAND_DIR(random_path, "$random_seed")
	AC_DEFINE_UNQUOTED(RANDOMSEED, "$random_path", [Improve random at boot by seeding it with sth from before.])])

# Control build with automake flags
AM_CONDITIONAL(STATIC,    [test "x$enable_static" = "xyes"])
AM_CONDITIONAL(INETD,     [test "x$enable_inetd" = "xyes"])
AM_CONDITIONAL(WATCHDOGD, [test "x$enable_watchdog" = "xyes"])

# Generate all files
AC_OUTPUT

cat <<EOF

------------------ Summary ------------------
 $PACKAGE_NAME version $PACKAGE_VERSION
  Prefix................: $prefix
  Sysconfdir............: `eval echo $sysconfdir`
  Localstatedir.........: `eval echo $localstatedir`
  FIFO path.............: $fifo_path
  Finit config file.....: $conf_path
  Finit config.d path...: $rcsd_path
  Compat rc.local path..: $rclocal_path
  Random seed path......: $random_path
  C Compiler............: $CC $CFLAGS $CPPFLAGS $LDFLAGS $LIBS
  Linker................: $LD $LLDP_LDFLAGS $LLDP_BIN_LDFLAGS $LDFLAGS $LIBS

Behavior:
  Boot heading..........: $heading
  Plugins...............: $plugins

Optional features:
  Built-in inetd........: $enable_inetd
  Built-in watchdogd....: $enable_watchdog
  Built-in logrotate....: $enable_logrotate
  Emergency shell.......: $enable_emergency_shell
  Fallback shell........: $enable_fallback_shell
  Remount / RW at boot..: $enable_rw_rootfs
  Traditional progress..: $enable_progress
  Default console dev...: $console
  Default hostname......: $hostname
  Default runlevel......: $runlevel

------------- Compiler version --------------
$($CC --version || true)
-------------- Linker version ---------------
$($LD --version || true)
---------------------------------------------

Check the above options and compile with:
 ${MAKE-make}

EOF
