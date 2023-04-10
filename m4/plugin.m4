# From https://github.com/collectd/collectd/blob/master/configure.ac
# Dependency handling currently unused

define([enadis], [ifelse([$1], [yes], [disable], [enable])])

# AC_PLUGIN(name, default, info)
# ------------------------------------------------------------
AC_DEFUN([AC_PLUGIN],[
    m4_divert_once([HELP_ENABLE], [
Optional Plugins:])
define(PLUGGY, m4_translit([$1],[a-z-][A-Z-]))dnl
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
    AC_DEFINE(HAVE_[]m4_translit($1,[-a-z],[_A-Z])_PLUGIN, 1, [Define to 1 if the $1 plugin is enabled.])
    plugins="$1 $plugins"
  fi

AM_CONDITIONAL(BUILD_[]m4_translit($1,[-a-z],[_A-Z])_PLUGIN, test "x$enable_plugin" = "xyes")])
