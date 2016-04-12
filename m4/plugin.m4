# From https://github.com/collectd/collectd/blob/master/configure.ac
# Dependency handling currently unused

m4_define([my_toupper], [m4_translit([$1], m4_defn([m4_cr_letters]), m4_defn([m4_cr_LETTERS]))])

# AC_PLUGIN(name, default, info)
# ------------------------------------------------------------
AC_DEFUN([AC_PLUGIN],[
    m4_divert_once([HELP_ENABLE], [
Optional Plugins:])
    enable_plugin="no"
    force="no"
    AC_ARG_ENABLE([$1], AS_HELP_STRING([--enable-$1-plugin], [$3]), [
    if test "x$enableval" = "xyes"; then
             enable_plugin="yes"
    else if test "x$enableval" = "xforce"; then
             enable_plugin="yes"
             force="yes"
    else
             enable_plugin="no (disabled on command line)"
    fi; fi], [
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
            if test "x$2" = "xyes" || test "x$force" = "xyes"; then
                    AC_DEFINE([HAVE_PLUGIN_]my_toupper([$1]), 1, [Define to 1 if the $1 plugin is enabled.])
                    if test "x$2" != "xyes"; then
                            dependency_warning="yes"
                    fi
            else # User passed "yes" but dependency checking yielded "no" => Dependency problem.
                    dependency_error="yes"
                    enable_plugin="no (dependency error)"
            fi
    fi
    AM_CONDITIONAL([BUILD_PLUGIN_]my_toupper([$1]), test "x$enable_plugin" = "xyes")
    enable_$1="$enable_plugin"])# AC_ARG_ENABLE
