AC_INIT(Finit, 4.0-beta1, https://github.com/troglobit/finit/issues, finit, https://troglobit.com/finit.html)
AM_INIT_AUTOMAKE([1.11 foreign dist-xz subdir-objects])
AM_SILENT_RULES([yes])

AC_CONFIG_SRCDIR([src/finit.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile
	contrib/Makefile
	contrib/alpine/Makefile contrib/alpine/finit.d/Makefile contrib/alpine/finit.d/available/Makefile
	contrib/debian/Makefile contrib/debian/finit.d/Makefile contrib/debian/finit.d/available/Makefile contrib/debian/grub.d/Makefile
	contrib/void/Makefile   contrib/void/finit.d/Makefile   contrib/void/finit.d/available/Makefile
	doc/Makefile
	plugins/Makefile
	src/Makefile
	test/test.env
	test/Makefile])

# Older versions of autoconf (<2.58) do not have AC_CONFIG_MACRO_DIR()
#m4_include([m4/plugin.m4])
#m4_include([m4/expand.m4])
AC_CONFIG_MACRO_DIR([m4])

# Handle building plugins either as dynamically loadable, or built-in
LT_INIT([shared disable-static dlopen aix-soname=both disable-fast-install])

# Checks for programs.
AC_PROG_CC
AC_PROG_LN_S
AC_PROG_INSTALL
AC_PROG_MKDIR_P

# Configuration.
AC_HEADER_STDC
AC_CHECK_HEADERS([fstaby.h termios.h])
AC_CHECK_FUNCS([strstr getopt getfsenty])

# Check for uint[8,16,32]_t
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Check for pkg-config first, warn if it's not installed
PKG_PROG_PKG_CONFIG

# Check for required libraries
PKG_CHECK_MODULES([uev],  [libuev >= 2.2.0])
PKG_CHECK_MODULES([lite], [libite >= 2.0.1])

# Check for configured Finit features
AC_ARG_ENABLE(emergency_shell,
	AS_HELP_STRING([--enable-emergency-shell], [Experimental emergency fallback at boot.
	                Use only for debugging very early boot problems with Finit]),,[
	enable_emergency_shell=no])

AC_ARG_ENABLE(fallback_shell,
        AS_HELP_STRING([--enable-fallback-shell], [Start a shell if no TTYs can start]),,[
	enable_fallback_shell=no])

AC_ARG_ENABLE(auto_reload,
        AS_HELP_STRING([--enable-auto-reload], [Automatic reload on any .conf change]),,[
	enable_auto_reload=no])

AC_ARG_ENABLE(watchdog,
        AS_HELP_STRING([--enable-watchdog], [Enable built-in watchdog, uses /dev/watchdog]),,[
	enable_watchdog=no])

AC_ARG_ENABLE(redirect,
        AS_HELP_STRING([--enable-redirect], [Redirect service output to /dev/null, default: no]),,[
	enable_redirect_output=no])

AC_ARG_ENABLE(logit,
     AS_HELP_STRING([--disable-logit], [Disable logit, used for log redirection, default: enable]),,[
     enable_logit=yes])

AC_ARG_ENABLE(logrotate,
        AS_HELP_STRING([--disable-logrotate], [Disable built-in rotation of /var/log/wtmp, default enabled]),,[
	enable_logrotate=yes])

AC_ARG_ENABLE(doc,
        AS_HELP_STRING([--disable-doc], [Disable build and install of doc/ section]),,[
	enable_doc=yes])

AC_ARG_ENABLE(contrib,
        AS_HELP_STRING([--disable-contrib], [Disable build and install of contrib section]),,[
	enable_contrib=yes])

# Check for extra plugins to enable
enable_all_plugins=auto
AC_PLUGIN([alsa-utils],    [no],  [Save and restore ALSA sound settings using alsactl])
AC_PLUGIN([dbus],          [no],  [Setup and start system message bus, D-Bus])
AC_PLUGIN([hotplug],       [yes], [Setup and start udev or mdev hotplug daemon])
AC_PLUGIN([modules-load],  [no],  [Scans /etc/modules-load.d for modules to load])
AC_PLUGIN([resolvconf],    [no],  [Setup necessary files for resolvconf])
AC_PLUGIN([x11-common],    [no],  [Console setup (for X)])
AC_PLUGIN([netlink],       [yes], [Basic netlink plugin for IFUP/IFDN and GW events. Can be replaced with externally built plugin that links with libnl or similar.])

# Check for extra arguments or packages
AC_ARG_WITH(heading,
        AS_HELP_STRING([--with-heading=GREET], [Display GREET instead of Finit version at boot]),
	[heading=$withval], [heading=yes])

AC_ARG_WITH(config,
        AS_HELP_STRING([--with-config=CONFIG], [Finit bootstrap config file, default /etc/finit.conf]),
	[conf=$withval], [conf=yes])

AC_ARG_WITH(rcsd,
        AS_HELP_STRING([--with-rcsd=DIR], [Finit rcS.d/ directory, default /etc/finit.d]),
	[rcsd=$withval], [rcsd=yes])

AC_ARG_WITH(rc-local,
        AS_HELP_STRING([--with-rc-local=FILE], [SysV init /etc/rc.local file, default /etc/rc.local]),
	[rclocal=$withval], [rclocal=yes])

AC_ARG_WITH(sysconfig,
        AS_HELP_STRING([--with-sysconfig=DIR], [Enviroment config directory, default /etc/default]),
	[sysconfig=$withval], [sysconfig=yes])

AC_ARG_WITH(hostname,
        AS_HELP_STRING([--with-hostname=NAME], [If /etc/hostname is missing, default: noname]),
	[hostname=$withval], [hostname=yes])

AC_ARG_WITH(runlevel,
        AS_HELP_STRING([--with-runlevel=N], [Runlevel to switch to after bootstrap, default: 2]),
	[runlevel=$withval], [runlevel=yes])

AC_ARG_WITH(random-seed,
        AS_HELP_STRING([--with-random-seed=FILE], [Save a random seed for /dev/urandom across reboots, default /var/lib/misc/random-seed]),
	[random_seed=$withval], [random_seed=yes])

### Enable features ###########################################################################

# Create config.h from selected features and fallback defautls
AS_IF([test "x$enable_emergency_shell" = "xyes"], [
        AC_DEFINE(EMERGENCY_SHELL,  1, [Fork off init and let PID 1 wait and start /bin/sh if init crashes.])])

AS_IF([test "x$enable_fallback_shell" = "xyes"], [
        AC_DEFINE(FALLBACK_SHELL,  1, [Start /bin/sh on console if no TTYs.])])

AS_IF([test "x$enable_auto_reload" = "xyes"], [
        AC_DEFINE(AUTO_RELOAD, 1, [Finit automatically does `initctl reload` on any .conf change])])

AS_IF([test "x$enable_watchdog" = "xyes"], [
        AC_DEFINE(BUILTIN_WATCHDOG,  1, [Enable built-in watchdog, kicks on /dev/watchdog])])

AS_IF([test "x$enable_redirect" = "xyes"], [
	AC_DEFINE(REDIRECT_OUTPUT, 1, [Enable redirection of service output to /dev/null])])

### Disable features ###########################################################################
AS_IF([test "x$enable_logit" != "xno"], [
	AC_DEFINE(LOGIT_ENABLED,  1, [Enable logit tool, used for log redirection by Finit])])

AS_IF([test "x$enable_logrotate" != "xno"], [
	AC_DEFINE(LOGROTATE_ENABLED, 1, [Enable built-in rotation of /var/log/wtmp et al.])])

### With features ##############################################################################
AS_IF([test "x$with_config" != "xno"], [
	AS_IF([test "x$conf" = "xyes"], [
		conf=$sysconfdir/finit.conf])])
AC_EXPAND_DIR(conf_path, "$conf")
AC_DEFINE_UNQUOTED(FINIT_CONF, "$conf_path", [Main Finit configuration file, default /etc/finit.conf])
AC_SUBST(FINIT_CONF, "$conf_path")

AS_IF([test "x$with_rcsd" != "xno"], [
	AS_IF([test "x$rcsd" = "xyes"], [
		rcsd=$sysconfdir/finit.d])])
AC_EXPAND_DIR(rcsd_path, "$rcsd")
AC_DEFINE_UNQUOTED(FINIT_RCSD, "$rcsd_path", [Finit rcS.d/, default /etc/finit.d])
AC_SUBST(FINIT_RCSD, "$rcsd_path")

AS_IF([test "x$with_rc_local" != "xno"], [
	AS_IF([test "x$rclocal" = "xyes"], [
		rclocal=$sysconfdir/rc.local])])
AC_EXPAND_DIR(rclocal_path, "$rclocal")
AC_DEFINE_UNQUOTED(FINIT_RC_LOCAL, "$rclocal_path", [Compat SysV /etc/rc.local file to run last at boot.])

AS_IF([test "x$with_sysconfig" != "xno"], [
	AS_IF([test "x$sysconfig" = "xyes"], [
		sysconfig=$sysconfdir/default])
	AC_EXPAND_DIR(sysconfig_path, "$sysconfig")
	AC_DEFINE_UNQUOTED(FINIT_SYSCONFIG, "$sysconfig_path",
			    [System environment, default /etc/default])])

AS_IF([test "x$with_heading" != "xno"], [
	AS_IF([test "x$heading" = "xyes"], [
		heading="$PACKAGE_NAME v$PACKAGE_VERSION"])])
AC_DEFINE_UNQUOTED(INIT_HEADING, "$heading", [Displayed by Finit at boot])

AS_IF([test "x$with_hostname" != "xno"], [
	AS_IF([test "x$hostname" = "xyes"], [
		hostname=noname])])
AC_DEFINE_UNQUOTED(DEFHOST, "$hostname", [For /etc/hostname])

AS_IF([test "x$with_runlevel" != "xno"], [
	AS_IF([test "x$runlevel" = "xyes"], [
		runlevel=2
		AC_DEFINE_UNQUOTED(RUNLEVEL, $runlevel, [Default runlevel to start after S])])
	AC_DEFINE_UNQUOTED(RUNLEVEL, $runlevel, [Runlevel to start after S])])

AS_IF([test "x$with_random_seed" != "xno"], [
	AS_IF([test "x$random_seed" = "xyes"], [
		random_seed=/var/lib/misc/random-seed])
	AC_EXPAND_DIR(random_path, "$random_seed")
	AC_DEFINE_UNQUOTED(RANDOMSEED, "$random_path", [Improve random at boot by seeding it with sth from before.])])

# Control build with automake flags
AM_CONDITIONAL(STATIC,    [test "x$enable_static" = "xyes"])
AM_CONDITIONAL(WATCHDOGD, [test "x$enable_watchdog" = "xyes"])
AM_CONDITIONAL(LOGIT,     [test "x$enable_logit" = "xyes"])
AM_CONDITIONAL(DOC,       [test "x$enable_doc" = "xyes"])
AM_CONDITIONAL(CONTRIB,   [test "x$enable_contrib" = "xyes"])

# Override default libdir, used for plugins and rescue.conf
pkglibdir=$libdir/finit
AC_SUBST(pkglibexecdir)
AC_EXPAND_DIR(pkglibexecdir, "$libexecdir/finit")
AC_DEFINE_UNQUOTED(FINIT_LIBPATH_, "$pkglibexecdir", [Finit libexec path])

AC_EXPAND_DIR(plugin_path, "$libdir/finit/plugins")
AC_SUBST(plugin_path)
AC_DEFINE_UNQUOTED(PLUGIN_PATH, "$plugin_path", [Finit plugin path])

AC_EXPAND_DIR(rescue_conf, "$libdir/finit/rescue.conf")
AC_DEFINE_UNQUOTED(RESCUE_CONF, "$rescue_conf", [Finit rescue mode .conf])

# Generate all files
AC_OUTPUT

cat <<EOF

------------------ Summary ------------------
 $PACKAGE_NAME version $PACKAGE_VERSION
  Prefix................: $prefix
  Exec prefix...........: $eprefix
  Sysconfdir............: `eval echo $sysconfdir`
  Localstatedir.........: `eval echo $localstatedir`
  Finit config file.....: $conf_path
  Finit config.d path...: $rcsd_path
  Finit plugin path.....: $plugin_path
  Compat rc.local path..: $rclocal_path
  System environment....: $sysconfig_path
  Random seed path......: $random_path
  C Compiler............: $CC $CFLAGS $CPPFLAGS $LDFLAGS $LIBS
  Linker................: $LD $LLDP_LDFLAGS $LLDP_BIN_LDFLAGS $LDFLAGS $LIBS

Behavior:
  Boot heading..........: $heading
  Plugins...............: $plugins

Optional features:
  Install doc/..........: $enable_doc
  Install contrib/......: $enable_contrib
  Built-in watchdogd....: $enable_watchdog
  Built-in logrotate....: $enable_logrotate
  Scripting tool logit..: $enable_logit
  Emergency shell.......: $enable_emergency_shell
  Fallback shell........: $enable_fallback_shell
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
