AC_INIT(Finit, 4.4-dev, https://github.com/troglobit/finit/issues, finit, https://troglobit.com/projects/finit/)
AC_CONFIG_AUX_DIR(aux)
AM_INIT_AUTOMAKE([1.11 foreign dist-xz subdir-objects])
AM_SILENT_RULES([yes])

AC_CONFIG_SRCDIR([src/finit.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile
	contrib/Makefile
	contrib/alpine/Makefile contrib/alpine/finit.d/Makefile contrib/alpine/finit.d/available/Makefile
	contrib/debian/Makefile contrib/debian/finit.d/Makefile contrib/debian/finit.d/available/Makefile
	contrib/void/Makefile   contrib/void/finit.d/Makefile   contrib/void/finit.d/available/Makefile
	doc/Makefile
	man/Makefile
	plugins/Makefile
	src/Makefile
	test/test.env test/Makefile
	test/tenv/Makefile test/common/Makefile])

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
AC_CHECK_HEADERS([termios.h sys/ioctl.h mntent.h sys/sysmacros.h])
AC_CHECK_FUNCS([strstr getopt getmntent getmntent_r])

# Check for uint[8,16,32]_t
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Check for pkg-config first, warn if it's not installed
PKG_PROG_PKG_CONFIG

# Check for required libraries
PKG_CHECK_MODULES([uev],  [libuev >= 2.2.0])
PKG_CHECK_MODULES([lite], [libite >= 2.2.0])

# Check for configured Finit features
AC_ARG_ENABLE(auto_reload,
        AS_HELP_STRING([--enable-auto-reload], [Automatic reload on any .conf change]),,[
	enable_auto_reload=no])

AC_ARG_ENABLE(kernel_cmdline,
        AS_HELP_STRING([--enable-kernel-cmdline], [Parse init args from /proc/cmdline (don't use!)]),,[
	enable_kernel_cmdline=no])

AC_ARG_ENABLE(fastboot,
        AS_HELP_STRING([--enable-fastboot], [Skip fsck check on filesystems listed in /etc/fstab]),,[
	enable_fastboot=no])

AC_ARG_ENABLE(fsckfix,
        AS_HELP_STRING([--enable-fsckfix], [Run fsck fix mode (options: -yf) on filesystems listed in /etc/fstab]),,[
	enable_fsckfix=no])

AC_ARG_ENABLE(redirect,
        AS_HELP_STRING([--disable-redirect], [Disable redirection of service output to /dev/null]),,[
	enable_redirect=yes])

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
AC_ARG_ENABLE(all_plugins,
    AS_HELP_STRING([--enable-all-plugins], [Enable all plugins, default: auto]),
	enable_all_plugins=$enableval, enable_all_plugins=auto)
AC_PLUGIN([alsa-utils],    [no],  [Save and restore ALSA sound settings using alsactl])
AC_PLUGIN([dbus],          [no],  [Setup and start system message bus, D-Bus])
AC_PLUGIN([mdevd],         [no],  [Setup and start system hotplug helper mdevd])
AC_PLUGIN([modules-load],  [no],  [Scans /etc/modules-load.d for modules to load])
AC_PLUGIN([modprobe],      [yes], [Coldplug modules using modalias magic])
AC_PLUGIN([resolvconf],    [no],  [Setup necessary files for resolvconf])
AC_PLUGIN([x11-common],    [no],  [Console setup (for X)])
AC_PLUGIN([netlink],       [yes], [Basic netlink plugin for IFUP/IFDN and GW events. Can be replaced with externally built plugin that links with libnl or similar.])
AC_PLUGIN([hook-scripts],  [no],  [Trigger script execution from hook points])
AC_PLUGIN([hotplug],       [yes], [Setup and start udev or mdev hotplug daemon])
AC_PLUGIN([rtc],           [yes], [Save and restore RTC using hwclock])
AC_PLUGIN([tty],           [yes], [Automatically activate new TTYs, e.g. USB-to-serial])
AC_PLUGIN([urandom],       [yes], [Setup and save random seed at boot/shutdown])

# Check for extra arguments or packages
AC_ARG_WITH(fstab,
        AS_HELP_STRING([--with-fstab=FILE], [System default fstab, default $sysconfdir/fstab]),
	[fstab=$withval], [fstab=yes])

AC_ARG_WITH(heading,
        AS_HELP_STRING([--with-heading=GREET], [Heading for boot progress. Default: PRETTY_NAME from /etc/os-release, fallback to "Finit vX.YY"]),
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
        AS_HELP_STRING([--with-sysconfig=DIR], [System environment directory, sourced by .conf files, default /etc/default & /etc/conf.d]),
	[sysconfig=$withval], [sysconfig=default])

AC_ARG_WITH(group,
	AS_HELP_STRING([--with-group=NAME], [Group for /run/finit/socket (initctl), default: root]),
	[group=$withval], [group=root])

AC_ARG_WITH(hostname,
        AS_HELP_STRING([--with-hostname=NAME], [If /etc/hostname is missing, default: noname]),
	[hostname=$withval], [hostname=yes])

AC_ARG_WITH(runlevel,
        AS_HELP_STRING([--with-runlevel=N], [Runlevel to switch to after bootstrap [1-9], default: 2]),
	[runlevel=$withval], [runlevel=yes])

AC_ARG_WITH(random-seed,
        AS_HELP_STRING([--with-random-seed=FILE], [Save a random seed for /dev/urandom across reboots, default /var/lib/misc/random-seed]),
	[random_seed=$withval], [random_seed=yes])

AC_ARG_WITH(keventd,
        AS_HELP_STRING([--with-keventd], [Enable built-in keventd, default: no]),, [with_keventd=no])

AC_ARG_WITH(sulogin,
        AS_HELP_STRING([--with-sulogin], [Enable built-in sulogin, default: no.]),, [with_sulogin=no])

AC_ARG_WITH(watchdog,
        AS_HELP_STRING([--with-watchdog=[DEV]], [Enable built-in watchdog, default: /dev/watchdog]),
	[watchdog=$withval], [with_watchdog=no watchdog=])

AC_ARG_WITH(hook-scripts-path,
        AS_HELP_STRING([--with-hook-scripts-path=DIR], [Base directory for hook scripts, default $libexecdir/finit/hook]),
	[hook_scripts_path=$withval], [hook_scripts_path=yes])

AC_ARG_WITH(plugin-path,
        AS_HELP_STRING([--with-plugin-path=DIR], [Search path for external plugins, default /usr/lib/finit/plugins:/usr/local/lib/finit/plugins]),
	[plugin_path=$withval], [plugin_path=yes])

### Enable features ###########################################################################

# Create config.h from selected features and fallback defaults
AS_IF([test "x$enable_auto_reload" = "xyes"], [
        AC_DEFINE(AUTO_RELOAD, 1, [Finit automatically does `initctl reload` on any .conf change])])

AS_IF([test "x$enable_kernel_cmdline" = "xyes"], [
        AC_DEFINE(KERNEL_CMDLINE, 1, [Dumpster diving after init args from /proc/cmdline])])

AS_IF([test "x$enable_fastboot" = "xyes"], [
	AC_DEFINE(FAST_BOOT, 1, [Skip fsck check on filesystems listed in /etc/fstab])])

AS_IF([test "x$enable_fsckfix" = "xyes"], [
	AC_DEFINE(FSCK_FIX, 1, [Run fsck fix mode (options: -yf) on filesystems listed in /etc/fstab])])

AS_IF([test "x$enable_redirect" = "xyes"], [
	AC_DEFINE(REDIRECT_OUTPUT, 1, [Enable redirection of service output to /dev/null])])

### Disable features ###########################################################################
AS_IF([test "x$enable_logrotate" != "xno"], [
	AC_DEFINE(LOGROTATE_ENABLED, 1, [Enable built-in rotation of /var/log/wtmp et al.])])

AM_CONDITIONAL(LOGROTATE, [test "x$enable_logrotate" = "xyes"])

### With features ##############################################################################
AS_IF([test "x$with_fstab" != "xno"], [
	AS_IF([test "x$fstab" = "xyes"], [fstab=$sysconfdir/fstab])
	AC_EXPAND_DIR(fstab_path, "$fstab")
	AC_DEFINE_UNQUOTED(FINIT_FSTAB, "$fstab_path", [Built-in default: /etc/fstab])],[
	AC_DEFINE_UNQUOTED(FINIT_FSTAB, NULL, [No built-in default fstab])])

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
	AS_IF([test "x$sysconfig" != "xdefault"], [
		    AC_EXPAND_DIR(sysconfig_path, "$sysconfig")
		    AC_DEFINE_UNQUOTED(FINIT_SYSCONFIG, "$sysconfig_path",
			    [System environment, default /etc/default & /etc/conf.d])])])

AS_IF([test "x$with_heading" != "xno"], [
	AS_IF([test "x$heading" = "xyes"], [heading=""])
	AC_DEFINE_UNQUOTED(INIT_OSHEADING, "$heading", [Displayed by Finit at boot, defaults to /etc/os-release PRETTY_NAME])])

AS_IF([test "x$with_group" != "xno"], [
	AS_IF([test "x$group" = "xyes"], [
		group=root])])
AC_DEFINE_UNQUOTED(DEFGROUP, "$group", [For /run/finit/socket])

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

AS_IF([test "x$with_keventd" != "xno"], [with_keventd=yes])

AS_IF([test "x$with_sulogin" != "xno"], [with_sulogin=yes])

AS_IF([test "x$with_watchdog" != "xno"], [
	AS_IF([test "x$watchdog" = "xyes"], [
		watchdog=/dev/watchdog])
	with_watchdog=yes
        AC_DEFINE_UNQUOTED(WDT_DEVNODE, "$watchdog", [Built-in watchdog device node])], [
	watchdog=])

AS_IF([test "x$enable_hook_scripts_plugin" != "xno"], [
	AS_IF([test "x$hook_scripts_path" = "xyes"], [hook_scripts_path=$libexecdir/finit/hook])
	AC_EXPAND_DIR(hook_scripts_path, "$hook_scripts_path")
	AC_DEFINE_UNQUOTED(PLUGIN_HOOK_SCRIPTS_PATH, "$hook_scripts_path", [Built-in default: $libexecdir/finit])])

AS_IF([test "x$with_plugin_path" != "xno"], [
	AS_IF([test "x$plugin_path" = "xyes"], [plugin_path=/usr/lib/finit/plugins:/usr/local/lib/finit/plugins])
	AC_EXPAND_DIR(plugin_path, "$plugin_path")
	AC_DEFINE_UNQUOTED(EXTERNAL_PLUGIN_PATH, "$plugin_path", [Built-in default: /usr/lib/finit/plugins:/usr/local/lib/finit/plugins])])

# Control build with automake flags
AM_CONDITIONAL(STATIC,    [test "x$enable_static" = "xyes"])
AM_CONDITIONAL(KEVENTD,   [test "x$with_keventd"  != "xno"])
AM_CONDITIONAL(SULOGIN,   [test "x$with_sulogin"  != "xno"])
AM_CONDITIONAL(WATCHDOGD, [test "x$with_watchdog" != "xno"])
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
AC_EXPAND_DIR(sample_conf, "$libdir/finit/sample.conf")
AC_DEFINE_UNQUOTED(SAMPLE_CONF, "$sample_conf", [Finit sameple.conf for initctl create])

AS_IF([test "$heading" = ""],   [heading="<DEFAULT>"])
AS_IF([test "$heading" = "no"], [heading="<NONE>"])

# Generate all files
AC_OUTPUT

cat <<EOF

------------------ Summary ------------------
 $PACKAGE_NAME version $PACKAGE_VERSION
  Prefix................: $prefix
  Exec prefix...........: $eprefix
  Sysconfdir............: `eval echo $sysconfdir`
  Localstatedir.........: `eval echo $localstatedir`
  Default fstab.........: `eval echo $fstab`
  Finit config file.....: $conf_path
  Finit config.d path...: $rcsd_path
  Finit plugin path.....: $plugin_path
  Compat rc.local path..: $rclocal_path
  System environment....: ${sysconfig_path:-${sysconfig}}
  Random seed path......: $random_path
  C Compiler............: $CC $CFLAGS $CPPFLAGS $LDFLAGS $LIBS
  Linker................: $LD $LLDP_LDFLAGS $LLDP_BIN_LDFLAGS $LDFLAGS $LIBS

Behavior:
  Boot heading..........: $heading
  Plugins...............: $plugins

Optional features:
  Install doc/..........: $enable_doc
  Install contrib/......: $enable_contrib
  Built-in keventd......: $with_keventd
  Built-in sulogin......: $with_sulogin
  Built-in watchdogd....: $with_watchdog $watchdog
  Built-in logrotate....: $enable_logrotate
  Skip fsck check.......: $enable_fastboot
  Run fsck fix mode.....: $enable_fsckfix
  Redirect output.......: $enable_redirect
  Default hostname......: $hostname
  Default group.........: $group
  Default runlevel......: $runlevel

------------- Compiler version --------------
$($CC --version || true)
-------------- Linker version ---------------
$($LD --version || true)
---------------------------------------------

Check the above options and compile with:
 ${MAKE-make}

EOF
