AC_INIT(blink.c)

AM_INIT_AUTOMAKE(blinkd,0.4.8)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE
AC_PROG_INTLTOOL(0.37, 1)

dnl AC_ARG_PROGRAM
AC_DEFINE(_GNU_SOURCE, 1, only for GNU/Linux)
AC_DEFINE(SERV_TCP_PORT, 20013, blinkd server TCP port)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_LN_S
AC_PROG_INSTALL

dnl Checks for libraries.
ALL_LINGUAS="de sv"
AM_GNU_GETTEXT([external])
GETTEXT_PACKAGE=blinkd
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h limits.h paths.h sys/ioctl.h sys/time.h syslog.h unistd.h pthread.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_SETPGRP
AC_TYPE_SIGNAL
AC_CHECK_FUNCS(socket)

AC_OUTPUT([Makefile po/Makefile.in])
