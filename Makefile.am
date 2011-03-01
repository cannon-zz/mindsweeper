SUBDIRS = pixmaps

INCLUDES = -I$(top_srcdir)/pixmaps -I$(top_srcdir)/pixmaps/counters/$(COUNTER)

bin_PROGRAMS = mindsweeper

mindsweeper_SOURCES = main.c mindsweeper.c minefield.c solver.c ui.c mindsweeper.h minefield.h solver.h ui.h
mindsweeper_CFLAGS = $(AM_CFLAGS) $(GTK_CFLAGS) -DPACKAGE_VERSION=\"$(PACKAGE_VERSION)\" $(DIAGFLAGS)
mindsweeper_LDADD = $(GTK_LIBS)

# legacy script for installing icon image, preserved for reference
desktopinstall :
	for x in $(DESTDIR){$(bindir),$(datadir)/{applications,pixmaps}} ; do [ -d $${x} ] || @INSTALL@ -m 0755 -d $${x} ; done
	@INSTALL@ -m 0755 $(TARGET) $(DESTDIR)$(bindir)
	@INSTALL@ -m 0644 mindsweeper.desktop $(DESTDIR)$(datadir)/applications
	@INSTALL@ -m 0644 pixmaps/mindsweeper.png $(DESTDIR)$(datadir)/pixmaps
