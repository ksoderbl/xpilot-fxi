bin_PROGRAMS = xpilot
AM_CPPFLAGS = -DCONF_DATADIR=\"$(pkgdatadir)/\" -I$(top_srcdir)/src/common -I$(top_srcdir)/src/client
xpilot_SOURCES = \
about.c caudio.c caudio.h client.c client.h colors.c configure.c configure.h connectparam.h datagram.c dbuff.c dbuff.h debugaudio.c default.c gfx2d.c gfx2d.h gfx3d.h guimap.c guimap.h guiobjects.c guiobjects.h icon.h join.c netclient.c netclient.h paint.c paintdata.c paintdata.h paint.h painthud.c paintmap.c paintobjects.c paintradar.c protoclient.h query.c record.c recordfmt.h record.h talk.c talk.h talkmacros.c textinterface.c usleep.c welcome.c widget.c widget.h xevent.c xeventhandlers.c xeventhandlers.h xinit.c xinit.h xpilot.c

xpilot_LDADD = $(top_builddir)/src/common/libxpcommon.a @X_LIBS@ @X_PRE_LIBS@ @X_EXTENSIONS_LIB@ @X_EXTRA_LIBS@ -lX11

SUBDIRS = items
