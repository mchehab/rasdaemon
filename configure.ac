AC_INIT([RASdaemon], 0.5.8)
AM_SILENT_RULES([yes])
AC_CANONICAL_SYSTEM
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

X_AC_META

AC_CONFIG_FILES([
	Makefile
	libtrace/Makefile
	man/Makefile
	man/ras-mc-ctl.8
	man/rasdaemon.1
	misc/rasdaemon.spec
	util/Makefile
	util/ras-mc-ctl
])

AC_ARG_ENABLE([sqlite3],
    AS_HELP_STRING([--enable-sqlite3], [enable storing data at SQL lite database (currently experimental)]))

AS_IF([test "x$enable_sqlite3" = "xyes"], [
  AC_CHECK_LIB(sqlite3, sqlite3_open,[echo "found sqlite3"] , AC_MSG_ERROR([*** Unable to find sqlite3 library]), )
  SQLITE3_LIBS="-lsqlite3"
  AC_DEFINE(HAVE_SQLITE3,1,"have sqlite3")
  AC_SUBST([WITH_SQLITE3])
])

AM_CONDITIONAL([WITH_SQLITE3], [test x$enable_sqlite3 = xyes])
AC_SUBST([SQLITE3_LIBS])


AC_ARG_ENABLE([aer],
    AS_HELP_STRING([--enable-aer], [enable PCIe AER events (currently experimental)]))

AS_IF([test "x$enable_aer" = "xyes"], [
  AC_DEFINE(HAVE_AER,1,"have PCIe AER events collect")
  AC_SUBST([WITH_AER])
])
AM_CONDITIONAL([WITH_AER], [test x$enable_aer = xyes])

AC_ARG_ENABLE([mce],
    AS_HELP_STRING([--enable-mce], [enable MCE events (currently experimental)]))

AS_IF([test "x$enable_mce" = "xyes"], [
  AC_DEFINE(HAVE_MCE,1,"have PCIe MCE events collect")
  AC_SUBST([WITH_MCE])
])
AM_CONDITIONAL([WITH_MCE], [test x$enable_mce = xyes])

AC_ARG_ENABLE([extlog],
    AS_HELP_STRING([--enable-extlog], [enable EXTLOG events (currently experimental)]))

AS_IF([test "x$enable_extlog" = "xyes"], [
  AC_DEFINE(HAVE_EXTLOG,1,"have EXTLOG events collect")
  AC_SUBST([WITH_EXTLOG])
])
AM_CONDITIONAL([WITH_EXTLOG], [test x$enable_extlog = xyes])

AC_ARG_ENABLE([abrt_report],
    AS_HELP_STRING([--enable-abrt-report], [enable report event to ABRT (currently experimental)]))

AS_IF([test "x$enable_abrt_report" = "xyes"], [
  AC_DEFINE(HAVE_ABRT_REPORT,1,"have report event to ABRT")
  AC_SUBST([WITH_ABRT_REPORT])
])
AM_CONDITIONAL([WITH_ABRT_REPORT], [test x$enable_abrt_report = xyes])

test "$sysconfdir" = '${prefix}/etc' && sysconfdir=/etc

CFLAGS="$CFLAGS -Wall -Wmissing-prototypes -Wstrict-prototypes"

AC_SUBST([rasstatedir], [$localstatedir/lib/rasdaemon])
AC_DEFINE_DIR([RASSTATEDIR], [rasstatedir], [rasdaemon db store state dir])
AC_SUBST([RASSTATEDIR])

AC_DEFINE([RAS_DB_FNAME], ["ras-mc_event.db"], [ras events database])
AC_SUBST([RAS_DB_FNAME], ["ras-mc_event.db"])

AC_OUTPUT
