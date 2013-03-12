AC_INIT([RAS Daemon], 0.1)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_FILES([Makefile libtrace/Makefile])
AM_INIT_AUTOMAKE
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_CHECK_LIB(sqlite3, sqlite3_open,[echo "found sqlite3"] , AC_MSG_ERROR([*** Unable to find sqlite3 library]), )

AC_OUTPUT
