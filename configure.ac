dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([XPilot], 1.2.0ix, foo@bar.com, xpilot)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_SRCDIR(src/client/xpilot.c)

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AM_CONFIG_HEADER(config.h)

dnl  Copyright & stuff.
XP_AUTHORS="Bjørn Stabell, Ken Ronny Schouten, Bert Gijsbers, Coppa"
AC_SUBST([XP_AUTHORS])

XP_COPYRIGHTDATE="1991-2005"
AC_SUBST([XP_COPYRIGHTDATE])

XP_COPYRIGHT="Copyright © $XP_COPYRIGHTDATE by $XP_AUTHORS"
AC_SUBST([XP_COPYRIGHT])

AC_SUBST([XP_RELEASEDATE],["Feb 5, 2005"])
AC_SUBST([XP_DATADIR],"$prefix/share/xpilot")


dnl  Checks for options.
dnl  We should have some general macro for these.
AC_ARG_ENABLE(dbe,
	AC_HELP_STRING([--enable-dbe],
			[support for X Doublebuffer Extension in client]),
[if test x$enable_dbe = xyes; then
	AC_DEFINE([DBE], 1, [support for X Doublebuffer Extension in client])
fi])

AC_ARG_ENABLE(mbx,
	AC_HELP_STRING([--enable-mbx],
			[support for X Multibuffer Extension in client]),
[if test x$enable_mbx = xyes; then
	AC_DEFINE([MBX], 1, [support for X Multibuffer Extension in client])
fi])

AC_ARG_ENABLE(plockserver,
	AC_HELP_STRING([--enable-plockserver],
			[lock the xpilot-ng server into memory]),
[if test x$enable_plockserver = xyes; then
	AC_DEFINE([PLOCKSERVER], 1, [lock the xpilot-ng server into memory])
fi])

AC_ARG_ENABLE(development,
	AC_HELP_STRING([--enable-development],
			[enable development code]),
[if test x$enable_development = xyes; then
	AC_DEFINE([DEVELOPMENT], 1, [enable development code])
fi])

dnl  Checks for programs.
AC_PROG_CC
dnl AC_PROG_GCC_TRADITIONAL
dnl AC_PROG_CPP
dnl AC_PROG_CXX
dnl AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_INSTALL

dnl  Checks for libraries.
dnl kps - these should be cleaned up and not use the autoconf internal
dnl shell variables.
AC_CHECK_LIB(m, cos)
if test x$ac_cv_lib_m_cos != xyes; then
	AC_MSG_ERROR([*** Math library not found!])
fi
AC_CHECK_LIB(expat, XML_ParserCreate)
if test x$ac_cv_lib_expat_XML_ParserCreate != xyes; then
        AC_MSG_ERROR([*** Required library Expat not found!])
fi  

AC_PATH_X
AC_PATH_XTRA
if test x$no_x == xyes; then
	AC_MSG_ERROR([*** Couldn't find X headers or libraries!])
fi

dnl kps - Hack to link with libXext if DBE or MBX is configured
if test x$enable_dbe = xyes || test x$enable_mbx = xyes; then
	X_EXTENSIONS_LIB="-lXext"
else
	X_EXTENSIONS_LIB=
fi
AC_SUBST(X_EXTENSIONS_LIB)

dnl Figure out which math library to use
dnl (borrowed from from http://www.libsdl.org/opengl/SDLgears-1.0.2.tar.gz)
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

dnl  Checks for header files.
dnl AC_HEADER_STDC
AC_HEADER_TIME
dnl AC_HEADER_STDBOOL
AC_CHECK_HEADERS([ \
	arpa/inet.h \
	arpa/nameser.h \
	assert.h \
	bstring.h \
	ctype.h \
	errno.h \
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
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T
AC_STRUCT_TM

dnl Check for socklen_t (in Unix98)
AC_MSG_CHECKING(for socklen_t)
AC_TRY_COMPILE([
	#include <sys/types.h>
	#include <sys/socket.h>
	socklen_t x;
], [],
[
	AC_MSG_RESULT(yes)
], [
	AC_TRY_COMPILE([
		#include <sys/types.h>
		#include <sys/socket.h>
		int accept(int, struct sockaddr *, size_t *);
	], [], [
		AC_MSG_RESULT(size_t)
		AC_DEFINE(socklen_t, size_t, [socklen_t size])
	], [
		AC_MSG_RESULT(int)
		AC_DEFINE(socklen_t, int, [socklen_t size])
	])
])

dnl  Checks for library functions.

dnl We probably don't need GNU compatible malloc and realloc functions,
dnl so these are disabled for now.
dnl AC_FUNC_MALLOC
dnl AC_FUNC_REALLOC

AC_FUNC_MEMCMP
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_SETVBUF_REVERSED
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
	src/client/Makefile \
	src/client/items/Makefile \
	src/server/Makefile \
	])
AC_OUTPUT

