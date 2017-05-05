AC_INIT([A self-playing minefield game],[1.10],[kcannon@users.sf.net],[mindsweeper])
AC_COPYRIGHT([Copyright (C) 2011 Kipp C. Cannon])
AC_CONFIG_AUX_DIR([gnuscripts])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_FILES([
	Makefile \
	mindsweeper.spec \
	desktop/Makefile \
	pixmaps/Makefile \
	pixmaps/counters/Makefile \
	pixmaps/counters/mindsweeper/Makefile \
])

AC_PROG_CC
AC_PROG_INSTALL
AC_SUBST([GTK_VERSION],[no])
AM_PATH_GTK_2_0([2.12.0],[GTK_VERSION=yes],,[gthread])

AC_SUBST([AM_CFLAGS], [-Wall])

AC_SUBST([DIAGFLAGS],[])
AC_ARG_ENABLE([diagnostics],AC_HELP_STRING([--enable-diagnostics],[Include statistics-gathering code]),[DIAGFLAGS=-DDIAGNOSTICS])

AC_SUBST([COUNTER],[mindsweeper])
AC_ARG_WITH([counter],AC_HELP_STRING([--with-counter=mindsweeper],[Choose the numeral set for the counters]),[COUNTER=${withval}])

AC_PROG_MAKE_SET
AC_OUTPUT
