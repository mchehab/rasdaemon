AC_INIT([RAS Daemon], 0.2.0)
AM_SILENT_RULES([yes])
AC_CANONICAL_SYSTEM
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile libtrace/Makefile util/Makefile util/ras-mc-ctl])
AM_INIT_AUTOMAKE
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

AC_ARG_ENABLE([sqlite3],
    AS_HELP_STRING([--enable-sqlite], [enable storing data at SQL lite database (currently experimental]))

AS_IF([test "x$enable_sqlite3" = "xyes"], [
  AC_CHECK_LIB(sqlite3, sqlite3_open,[echo "found sqlite3"] , AC_MSG_ERROR([*** Unable to find sqlite3 library]), )
  SQLITE3_LIBS="-lsqlite3"
  AC_DEFINE(HAVE_SQLITE3,1,"have sqlite3")
  AC_SUBST([WITH_SQLITE3])
])

AM_CONDITIONAL([WITH_SQLITE3], [test x$enable_sqlite3 = xyes])
AC_SUBST([SQLITE3_LIBS])

AC_OUTPUT
