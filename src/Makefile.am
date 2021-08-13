# Makefile.am, was previously Makefile.in (before 2021)
#
# Copyright 1999 Niels Provos <provos@citi.umich.edu>
# Copyright 2018 Frédéric Bonnard <frediz@debian.org>
# Copyright 2019 Helmut Grohne <helmut@subdivi.de>
# Copyright 2021 Joao Eriberto Mota Filho <eriberto@eriberto.pro.br>
#
# This file is under the same license of the outguess.

./jpeg-6b-steg/libjpeg.a:
	$(MAKE) -C ./jpeg-6b-steg -f makefile.ansi 'CC=$(CC)' CFLAGS+=-DHAVE_STDC_HEADERS libjpeg.a

bin_PROGRAMS = outguess histogram

outguess_DEPENDENCIES = ./jpeg-6b-steg/libjpeg.a

outguess_SOURCES = outguess.c \
                   golay.c golay.h \
                   arc.c arc.h \
                   pnm.c pnm.h \
                   jpg.c jpg.h \
                   iterator.c iterator.h

if MD5MISS
outguess_SOURCES += md5.c md5.h
endif

if ERRMISS
outguess_SOURCES += err.c err.h
endif

outguess_LDADD = jpeg-6b-steg/libjpeg.a -lm

CLEANFILES = jpeg-6b-steg/*.o jpeg-6b-steg/libjpeg.a

histogram_SOURCES = histogram.c

# Install seek_script
dist_bin_SCRIPTS = seek_script
