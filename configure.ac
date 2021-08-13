dnl Copyright ?         dugsong@monkey.org and provos@citi.umich.edu
dnl Copyright 2016-2021 Joao Eriberto Mota Filho <eriberto@debian.org>
dnl
dnl This file is under the same license of the outguess.

AC_PREREQ([2.69])
AC_INIT([outguess],[VERSION],[https://github.com/resurrecting-open-source-projects/outguess/issues])
AC_CONFIG_SRCDIR([src/golay.h])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_RANLIB

# Check for jconfig.h in jpeg dir
jpegdir="src/jpeg-6b-steg"

AC_ARG_WITH([generic-jconfig],
            [Use a default jconfig.h in src/{jpegdir}],
            AC_CONFIG_COMMANDS([jconfig.h], [cd $jpegdir; cp jconfig.cfg jconfig.h], [jpegdir=$jpegdir]),
            AC_CHECK_FILE([$jpegdir]/jconfig.h, [],
                AC_MSG_ERROR(m4_normalize([Could not find jconfig.h in [$jpegdir]/.
                             Please provide a jconfig.h file or pass --with-generic-jconfig
                             to `configure' command to use a generic file.
                             See [$jpegdir]/install.doc for details.]))))

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h malloc.h netinet/in.h stddef.h stdlib.h string.h strings.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memmove memset munmap sqrt strcasecmp strchr strerror strrchr])

AC_SUBST(MD5MISS)
AC_SUBST(ERRMISS)

dnl Check if we need to compile md5
needmd5=no
needincmiss=no
AC_CHECK_FUNCS(MD5Update, , [needmd5=yes; needincmiss=yes])
if test $needmd5 = yes; then
   MD5MISS="true"
fi
AM_CONDITIONAL([MD5MISS], [test x$MD5MISS = xtrue])

dnl Check if there is err()
neederr=no
AC_CHECK_FUNCS(warnx, , [neederr=yes])
if test $neederr = yes; then
   ERRMISS="true"
fi
AM_CONDITIONAL([ERRMISS], [test x$ERRMISS = xtrue])

AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile])
AC_OUTPUT
