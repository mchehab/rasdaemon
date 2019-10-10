AC_INIT([RASdaemon], 0.6.4)
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

AC_ARG_ENABLE([all],
    AS_HELP_STRING([--enable-all], [enable all features]))

AC_ARG_ENABLE([sqlite3],
    AS_HELP_STRING([--enable-sqlite3], [enable storing data at SQL lite database (currently experimental)]))

AS_IF([test "x$enable_sqlite3" = "xyes" || test "x$enable_all" == "xyes"], [
  AC_CHECK_LIB(sqlite3, sqlite3_open,[echo "found sqlite3"] , AC_MSG_ERROR([*** Unable to find sqlite3 library]), )
  SQLITE3_LIBS="-lsqlite3"
  AC_DEFINE(HAVE_SQLITE3,1,"have sqlite3")
  AC_SUBST([WITH_SQLITE3])
])

AM_CONDITIONAL([WITH_SQLITE3], [test x$enable_sqlite3 = xyes || test x$enable_all == xyes])
AM_COND_IF([WITH_SQLITE3], [USE_SQLITE3="yes"], [USE_SQLITE3="no"])

AC_SUBST([SQLITE3_LIBS])


AC_ARG_ENABLE([aer],
    AS_HELP_STRING([--enable-aer], [enable PCIe AER events (currently experimental)]))

AS_IF([test "x$enable_aer" = "xyes" || test "x$enable_all" == "xyes"], [
  AC_DEFINE(HAVE_AER,1,"have PCIe AER events collect")
  AC_SUBST([WITH_AER])
])
AM_CONDITIONAL([WITH_AER], [test x$enable_aer = xyes || test x$enable_all == xyes])
AM_COND_IF([WITH_AER], [USE_AER="yes"], [USE_AER="no"])


AC_ARG_ENABLE([non_standard],
    AS_HELP_STRING([--enable-non-standard], [enable NON_STANDARD events (currently experimental)]))

AS_IF([test "x$enable_non_standard" = "xyes" || test "x$enable_all" == "xyes"], [
  AC_DEFINE(HAVE_NON_STANDARD,1,"have UNKNOWN_SEC events collect")
  AC_SUBST([WITH_NON_STANDARD])
])
AM_CONDITIONAL([WITH_NON_STANDARD], [test x$enable_non_standard = xyes || test x$enable_all == xyes])
AM_COND_IF([WITH_NON_STANDARD], [USE_NON_STANDARD="yes"], [USE_NON_STANDARD="no"])

AC_ARG_ENABLE([arm],
    AS_HELP_STRING([--enable-arm], [enable ARM events (currently experimental)]))

AS_IF([test "x$enable_arm" = "xyes" || test "x$enable_all" == "xyes"], [
  AC_DEFINE(HAVE_ARM,1,"have ARM events collect")
  AC_SUBST([WITH_ARM])
])
AM_CONDITIONAL([WITH_ARM], [test x$enable_arm = xyes || test x$enable_all == xyes])
AM_COND_IF([WITH_ARM], [USE_ARM="yes"], [USE_ARM="no"])

AC_ARG_ENABLE([mce],
    AS_HELP_STRING([--enable-mce], [enable MCE events (currently experimental)]))

AS_IF([test "x$enable_mce" = "xyes" || test "x$enable_all" == "xyes"], [
  AC_DEFINE(HAVE_MCE,1,"have PCIe MCE events collect")
  AC_SUBST([WITH_MCE])
])
AM_CONDITIONAL([WITH_MCE], [test x$enable_mce = xyes || test x$enable_all == xyes])
AM_COND_IF([WITH_MCE], [USE_MCE="yes"], [USE_MCE="no"])

AC_ARG_ENABLE([extlog],
    AS_HELP_STRING([--enable-extlog], [enable EXTLOG events (currently experimental)]))

AS_IF([test "x$enable_extlog" = "xyes" || test "x$enable_all" == "xyes"], [
  AC_DEFINE(HAVE_EXTLOG,1,"have EXTLOG events collect")
  AC_SUBST([WITH_EXTLOG])
])
AM_CONDITIONAL([WITH_EXTLOG], [test x$enable_extlog = xyes || test x$enable_all == xyes])
AM_COND_IF([WITH_EXTLOG], [USE_EXTLOG="yes"], [USE_EXTLOG="no"])

AC_ARG_ENABLE([devlink],
    AS_HELP_STRING([--enable-devlink], [enable devlink health events (currently experimental)]))

AS_IF([test "x$enable_devlink" = "xyes" || test "x$enable_all" == "xyes"], [
  AC_DEFINE(HAVE_DEVLINK,1,"have devlink health events collect")
  AC_SUBST([WITH_DEVLINK])
])
AM_CONDITIONAL([WITH_DEVLINK], [test x$enable_devlink = xyes || test x$enable_all == xyes])
AM_COND_IF([WITH_DEVLINK], [USE_DEVLINK="yes"], [USE_DEVLINK="no"])

AC_ARG_ENABLE([diskerror],
    AS_HELP_STRING([--enable-diskerror], [enable disk I/O error events (currently experimental)]))

AS_IF([test "x$enable_diskerror" = "xyes" || test "x$enable_all" == "xyes"], [
  AC_DEFINE(HAVE_DISKERROR,1,"have disk I/O errors collect")
  AC_SUBST([WITH_DISKERROR])
])
AM_CONDITIONAL([WITH_DISKERROR], [test x$enable_diskerror = xyes || test x$enable_all == xyes])
AM_COND_IF([WITH_DISKERROR], [USE_DISKERROR="yes"], [USE_DISKERROR="no"])

AC_ARG_ENABLE([abrt_report],
    AS_HELP_STRING([--enable-abrt-report], [enable report event to ABRT (currently experimental)]))

AS_IF([test "x$enable_abrt_report" = "xyes" || test "x$enable_all" == "xyes"], [
  AC_DEFINE(HAVE_ABRT_REPORT,1,"have report event to ABRT")
  AC_SUBST([WITH_ABRT_REPORT])
])
AM_CONDITIONAL([WITH_ABRT_REPORT], [test x$enable_abrt_report = xyes || test x$enable_all == xyes])
AM_COND_IF([WITH_ABRT_REPORT], [USE_ABRT_REPORT="yes"], [USE_ABRT_REPORT="no"])

AC_ARG_ENABLE([hisi_ns_decode],
    AS_HELP_STRING([--enable-hisi-ns-decode], [enable HISI_NS_DECODE events (currently experimental)]))

AS_IF([test "x$enable_hisi_ns_decode" = "xyes" || test "x$enable_all" == "xyes"], [
  AC_DEFINE(HAVE_HISI_NS_DECODE,1,"have HISI UNKNOWN_SEC events decode")
  AC_SUBST([WITH_HISI_NS_DECODE])
])
AM_CONDITIONAL([WITH_HISI_NS_DECODE], [test x$enable_hisi_ns_decode = xyes || test x$enable_all == xyes])
AM_COND_IF([WITH_HISI_NS_DECODE], [USE_HISI_NS_DECODE="yes"], [USE_HISI_NS_DECODE="no"])

test "$sysconfdir" = '${prefix}/etc' && sysconfdir=/etc

CFLAGS="$CFLAGS -Wall -Wmissing-prototypes -Wstrict-prototypes"

AC_SUBST([rasstatedir], [$localstatedir/lib/rasdaemon])
AC_DEFINE_DIR([RASSTATEDIR], [rasstatedir], [rasdaemon db store state dir])
AC_SUBST([RASSTATEDIR])

AC_DEFINE([RAS_DB_FNAME], ["ras-mc_event.db"], [ras events database])
AC_SUBST([RAS_DB_FNAME], ["ras-mc_event.db"])

AC_OUTPUT

dnl ---------------------------------------------------------------------
dnl compile time options summary

cat <<EOF

compile time options summary
============================

    Sqlite3             : $USE_SQLITE3
    AER                 : $USE_AER
    MCE                 : $USE_MCE
    EXTLOG              : $USE_EXTLOG
    CPER non-standard   : $USE_NON_STANDARD
    ABRT report         : $USE_ABRT_REPORT
    HIP07 SAS HW errors : $USE_HISI_NS_DECODE
    ARM events          : $USE_ARM
    DEVLINK             : $USE_DEVLINK
    Disk I/O errors     : $USE_DISKERROR
EOF
