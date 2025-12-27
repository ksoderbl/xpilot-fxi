AM_CPPFLAGS = -DCONF_DATADIR=\"$(pkgdatadir)/\" -I$(top_srcdir)/src/common
bin_PROGRAMS = xpilots
xpilots_SOURCES = \
click.h cmdline.c collision.c command.c contact.c defaults.h event.c fileparser.c fileparser.h frame.c global.h id.c map.c map.h metaserver.c metaserver.h netserver.c netserver.h object.h objpos.c objpos.h option.c parser.c parser.h performance.c play.c player.c proto.h rank.c rank.h robot.c robotdef.c robot.h rules.c sched.c sched.h score.h server.c serverconst.h server.h ship.c shot.c tuner.c tuner.h update.c walls.c walls.h

xpilots_LDADD =  $(top_builddir)/src/common/libxpcommon.a
