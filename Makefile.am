SUBDIRS = desktop pixmaps

INCLUDES = -I$(top_srcdir)/pixmaps -I$(top_srcdir)/pixmaps/counters/$(COUNTER)

bin_PROGRAMS = mindsweeper

mindsweeper_SOURCES = main.c mindsweeper.c minefield.c solver.c ui.c mindsweeper.h minefield.h solver.h ui.h
mindsweeper_CFLAGS = $(AM_CFLAGS) $(GTK_CFLAGS) -DPACKAGE_VERSION=\"$(PACKAGE_VERSION)\" $(DIAGFLAGS)
mindsweeper_LDADD = $(GTK_LIBS)
