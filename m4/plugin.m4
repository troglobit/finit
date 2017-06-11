# From https://github.com/collectd/collectd/blob/master/configure.ac
# Dependency handling currently unused

define([toupper], [translit([$1], [a-z], [A-Z])])
define([sanitize], [translit([$1], [-], [_])])
define([enadis], [ifelse([$1], [yes], [disable], [enable])])

# AC_PLUGIN(name, default, info)
# ------------------------------------------------------------
AC_DEFUN([AC_PLUGIN],[
    m4_divert_once([HELP_ENABLE], [
Optional Plugins:])
    enable_plugin="no"
    define(pluggy, sanitize($1))
    define(PLUGGY, toupper(pluggy))
    AC_ARG_ENABLE([$1-plugin], AS_HELP_STRING([--enadis($2)-$1-plugin], [$3]), [
    if test "x$enableval" = "xyes"; then
             enable_plugin="yes"
    else
             enable_plugin="no"
    fi], [
    if test "x$enable_all_plugins" = "xauto"; then
             if test "x$2" = "xyes"; then
                     enable_plugin="yes"
             else
                     enable_plugin="no"
             fi
    else
             enable_plugin="$enable_all_plugins"
    fi])
    if test "x$enable_plugin" = "xyes"; then
       	    AC_DEFINE([HAVE_]PLUGGY[_PLUGIN], 1, [Define to 1 if the $1 plugin is enabled.])
	    plugins="$1 $plugins"
    fi
    AM_CONDITIONAL([BUILD_]PLUGGY[_PLUGIN], test "x$enable_plugin" = "xyes")
    enable_pluggy="$enable_plugin"])
