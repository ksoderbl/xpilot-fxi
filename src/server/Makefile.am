AM_CPPFLAGS = -DCONF_DATADIR=\"$(pkgdatadir)/\" -I$(top_srcdir)/src/common
bin_PROGRAMS = xpilots
xpilots_SOURCES = \
cmdline.c collision.c collision.h command.c contact.c defaults.h event.c fileparser.c fileparser.h frame.c frame.h global.h id.c map.c map.h metaserver.c metaserver.h netserver.c netserver.h object.c object.h objpos.c objpos.h option.c parser.c parser.h performance.c player.c player.h player_inline.h proto.h rank.c rank.h robot.c robotdef.c robot.h rules.c sched.c sched.h score.c score.h server.c serverconst.h server.h ship.c ship.h shot.c team.c team.h tuner.c tuner.h update.c update.h walls.c walls.h

xpilots_LDADD =  $(top_builddir)/src/common/libxpcommon.a
