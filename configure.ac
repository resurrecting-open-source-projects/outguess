# autoconf.ac - based on configure.scan from 'autoscan'

# Copyright 1999-2000 Niels Provos <provos@citi.umich.edu>
# Copyright         ? Dug Song <dugsong@monkey.org>
# Copyright      2018 Joao Eriberto Mota Filho <eriberto@eriberto.pro.br>
# This file is under same license of outguess.

AC_PREREQ([2.67])
AC_INIT([outguess], [0.3], [https://github.com/eribertomota/outguess/issues])
AC_CONFIG_SRCDIR([outguess.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h netinet/in.h stdlib.h string.h unistd.h md5.h err.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memmove memset munmap sqrt strcasecmp strchr strerror strrchr])

#-------
# From OLD configure.in (outguess 0.2)
AC_SUBST(LIBMISSING)
AC_SUBST(MISSINGOBJ)
dnl Check if we need to compile md5
needmd5=no
needincmiss=no
AC_CHECK_FUNCS(MD5Update, , [needmd5=yes; needincmiss=yes])
if test $needmd5 = yes; then
   LIBMISSING="$LIBMISSING md5.o"
   MISSINGOBJ="$MISSINGOBJ missing-files/md5.c"
fi
dnl Check if there is err()
neederr=no
AC_CHECK_FUNCS(warnx, , [neederr=yes; needincmiss=yes])
if test $neederr = yes; then
   LIBMISSING="$LIBMISSING err.o"
   MISSINGOBJ="$MISSINGOBJ missing-files/err.c"
fi

if test $needincmiss = yes; then
   CFLAGS="$CFLAGS -Imissing-files"
fi
#-------

AC_CONFIG_SUBDIRS([jpeg-9c])
AC_CONFIG_FILES([Makefile])
AM_INIT_AUTOMAKE([foreign])

AC_OUTPUT
