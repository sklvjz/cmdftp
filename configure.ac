#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([cmdftp], [0.9.4], [sick_soul@users.sourceforge.net])

AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADER([config.h])

# Custom configure options

AC_MSG_NOTICE([checking custom features and packages])

AC_MSG_CHECKING([--enable-list-hack])
AC_ARG_ENABLE([list-hack], [AC_HELP_STRING([--enable-list-hack], [enable a workaround for servers with no or broken support for parameters to LIST and NLST commands (default:yes)])],
	    [
	      AC_MSG_RESULT($enable_list_hack)
	    ],
	    [
	      enable_list_hack=yes
	      AC_MSG_RESULT($enable_list_hack)
	    ]
)

if test "x$enable_list_hack" = "xyes"; then
  AC_DEFINE([CMDFTP_LIST_HACK], [1], [Define to enable workaround for spaces in NLST and servers not accepting parameters to LIST and NLST.])
fi

# Checks for programs.
AC_PROG_CC

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_STAT

AC_CHECK_HEADERS([stddef.h sys/socket.h netinet/in.h arpa/inet.h netdb.h termios.h signal.h ctype.h stdio.h libgen.h fnmatch.h errno.h fcntl.h getopt.h])

# Checks for typedefs, functions, compiler characteristics etc

AC_CHECK_FUNCS([strchr memcpy snprintf strdup mkstemp])

AC_C_VOLATILE
AC_TYPE_SIZE_T
AC_CHECK_TYPE(ssize_t, int)
AC_TYPE_PID_T
AC_TYPE_OFF_T
AC_CHECK_TYPE(socklen_t,,
	[
	  AC_DEFINE([socklen_t], [int], [define to `int' if <sys/types.h> and <sys/socket.h> do not define.])
	], [
	  #if HAVE_SYS_TYPES_H
	  # include <sys/types.h>
	  #endif

	  #if HAVE_SYS_SOCKET_H
	  # include <sys/socket.h>
	  #endif
	])
	  
AC_CHECK_TYPE(ptrdiff_t, int)

AC_CHECK_FUNC(fseeko, [
  AC_FUNC_FSEEKO
  AC_SYS_LARGEFILE
], [
  AC_MSG_WARN([using fseek as replacement to fseeko])
])

AC_CHECK_SIZEOF(off_t)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)

# Checks for libraries.

AC_SEARCH_LIBS(gethostbyname, nsl)
AC_SEARCH_LIBS(socket, socket, ,
    [AC_CHECK_LIB(nsl, socket, LIBS="$LIBS -lsocket -lnsl", , -lsocket)]
)

AC_CHECK_FUNCS([getopt_long],, [
  AC_CHECK_LIB([gnugetopt],[getopt_long], [
    AC_DEFINE([HAVE_GETOPT_LONG])
  ])
])

AC_OUTPUT([Makefile])
AC_MSG_NOTICE([Configuration complete.])
AC_MSG_NOTICE([CFLAGS=$CFLAGS, LDFLAGS=$LDFLAGS, LIBS=$LIBS])
AC_MSG_NOTICE([Installation prefix=$prefix])
