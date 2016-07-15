# From https://github.com/kakaroto/e17/blob/master/BINDINGS/javascript/elixir/m4/ac_expand_dir.m4
# By Alexandre Oliva <oliva@dcc.unicamp.br>

# AC_EXPAND_DIR(VARNAME, DIR)
# ------------------------------------------------------------
# Expands occurrences of ${prefix} and ${exec_prefix} in the
# given DIR, and assigns the resulting string to VARNAME
#
# Example: AC_DEFINE_DIR(DATADIR, "$datadir")
AC_DEFUN([AC_EXPAND_DIR], [
	$1=$2
	$1=`(
	    test "x$prefix" = xNONE && prefix="$ac_default_prefix"
	    test "x$exec_prefix" = xNONE && exec_prefix="${prefix}"
	    eval echo \""[$]$1"\"
	    )`
])
