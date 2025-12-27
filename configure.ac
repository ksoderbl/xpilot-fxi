dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.72])
AC_INIT([XPilot],[1.4.6fxi],[kps@users.sourceforge.net],[xpilot])

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_SRCDIR([src/server/walls.c])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_MAINTAINER_MODE

dnl  Copyright & stuff.
XP_AUTHORS="Bjoern Stabell, Ken Ronny Schouten, Bert Gijsbers, Dick Balaska, Kristian Soderblom, et al"
AC_SUBST([XP_AUTHORS])

XP_COPYRIGHTDATE="1991-2025"
AC_SUBST([XP_COPYRIGHTDATE])

XP_COPYRIGHT="Copyright (C) $XP_COPYRIGHTDATE by $XP_AUTHORS"
AC_SUBST([XP_COPYRIGHT])

AC_SUBST([XP_RELEASEDATE],["Apr 1, 2009"])
AC_SUBST([XP_DATADIR],"$prefix/share/xpilot")


dnl  Checks for options.
dnl  We should have some general macro for these.
AC_ARG_ENABLE(plockserver,
	AS_HELP_STRING([--enable-plockserver],[lock the server into memory]),
[if test x$enable_plockserver = xyes; then
	AC_DEFINE([PLOCKSERVER], 1, [lock the server into memory])
fi])

AC_ARG_ENABLE(development,
	AS_HELP_STRING([--enable-development],[enable development code]),
[if test x$enable_development = xyes; then
	AC_DEFINE([DEVELOPMENT], 1, [enable development code])
fi])

dnl  Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_RANLIB

dnl Figure out which math library to use
case "$target" in
    *-*-mingw32*)
        MATHLIB=""
        SYS_GL_LIBS="-lopengl32 -lglu32"
        ;;
    *-*-beos*)
        MATHLIB=""
        SYS_GL_LIBS="-lGL"
        ;;
    *-*-linux*)
        MATHLIB="-lm"
        SYS_GL_LIBS="-L/usr/X11R6/lib -lGL -lGLU"
        ;;
    *-*-freebsd*)
        MATHLIB="-lm"
        SYS_GL_LIBS="-L/usr/X11R6/lib -lGL -lGLU"
		CPPFLAGS="$CPPFLAGS -I/usr/X11R6/include -I/usr/local/include"
		LDFLAGS="$LDFLAGS -L/usr/local/lib"
        ;;
    *)
        MATHLIB="-lm"
        SYS_GL_LIBS="-lGL -lGLU"
        ;;
esac

dnl  Checks for libraries.
AC_CHECK_LIB([m], [cos], [], [AC_MSG_ERROR([*** Math library not found!])])
AC_CHECK_LIB([expat], [XML_ParserCreate], [], [AC_MSG_ERROR([*** Required library Expat not found!])])

dnl  Checks for header files.

dnl AC_HEADER_STDBOOL
AC_CHECK_HEADERS([ \
	arpa/inet.h \
	arpa/nameser.h \
	assert.h \
	bstring.h \
	ctype.h \
	errno.h \
    execinfo.h \
	fcntl.h \
	float.h \
	limits.h \
	math.h \
	net/if.h \
	netdb.h \
	netinet/in.h \
	netinet/tcp.h \
	pwd.h \
	resolv.h \
	setjmp.h \
	signal.h \
	stdarg.h \
	stddef.h \
	stdint.h \
	sys/file.h \
	sys/filio.h \
	sys/fcntl.h \
	sys/inttypes.h \
	sys/ioctl.h \
	sys/lock.h \
	sys/mman.h \
	sys/param.h \
	sys/socket.h \
	sys/sockio.h \
	sys/time.h \
	values.h \
	])

dnl 	X11/extensions/Xdbe.h
dnl	X11/extensions/multibuf.h
dnl	X11/X.h
dnl	X11/Xlib.h
dnl	X11/Xos.h
dnl	X11/Xutil.h
dnl	X11/keysym.h
dnl	X11/Xatom.h
dnl	X11/Xmd.h


dnl  Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE
AC_TYPE_MODE_T
AC_TYPE_SIZE_T
AC_STRUCT_TM

dnl  Checks for library functions.

dnl We probably don't need GNU compatible malloc and realloc functions,
dnl so these are disabled for now.
dnl AC_FUNC_MALLOC
dnl AC_FUNC_REALLOC

AC_FUNC_MEMCMP
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_FUNC_STRTOD
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([ \
	alarm \
	floor \
	gethostbyaddr \
	gethostbyname \
	gethostname \
	gettimeofday \
	inet_ntoa \
	isascii \
	memchr \
	memmove \
	memset \
	mkdir \
	pow \
	rint \
	select \
	socket \
	sqrt \
	strcasecmp \
	strchr \
	strdup \
	strerror \
	strlcat \
	strlcpy \
	strncasecmp \
	strpbrk \
	strrchr \
	strspn \
	strstr \
	strtol \
	strtoul \
	])

AC_CONFIG_FILES([ \
	Makefile \
	share/Makefile \
	share/maps/Makefile \
	src/Makefile \
	src/common/Makefile \
	src/common/version.h \
	src/server/Makefile \
	])
AC_OUTPUT
