# automake.am - based in original automake.in, from outguess 0.2

# Copyright 1999 Niels Provos <provos@citi.umich.edu>
# Copyright 2018 Joao Eriberto Mota Filho <eriberto@eriberto.pro.br>
# This file is under same license of outguess.

# Initial variables

JPEGDIR  = jpeg-9c
JPEGINCS = -I$(JPEGDIR)
JPEGLIBS = -L$(JPEGDIR) -ljpeg

MISSING  = @LIBMISSING@

# Firstly, we need a static library from libjpeg (libjpeg.a) and other elements.
# So, the build process must be personalized.

BUILT_SOURCES = libjpeg-static $(MISSING)

libjpeg-static:
	$(MAKE) -C $(JPEGDIR) -f makefile.ansi CFLAGS+=-DHAVE_STDC_HEADERS libjpeg.a

$(MISSING):
	$(CC) $(CFLAGS) $(INCS) -c @MISSINGOBJ@

# Now, going to outguess

bin_PROGRAMS = outguess histogram

outguess_SOURCES = outguess.c golay.c arc.c pnm.c jpg.c iterator.c
man_MANS = outguess.1
outguess_LDADD = $(INCS) $(MISSING) $(JPEGLIBS) -lm

# histogram

histogram_SOURCES = histogram.c
histogram_LDADD = $(INCS) $(MISSING)

# Cleaning

distclean-local:
	-rm -rf autom4te.cache/

	-rm -f *~ \
	aclocal.m4 \
	compile \
	config.* \
	configure \
	depcomp \
	install-sh \
	Makefile.in \
	missing

# Cleaning libjpeg dir corretly

	$(MAKE) -C $(JPEGDIR) distclean

	-rm -rf $(JPEGDIR)/autom4te.cache/

	-rm -f \
	$(JPEGDIR)/aclocal.m4 \
	$(JPEGDIR)/compile \
	$(JPEGDIR)/config.* \
	$(JPEGDIR)/configure \
	$(JPEGDIR)/depcomp \
	$(JPEGDIR)/install-sh \
	$(JPEGDIR)/Makefile.in \
	$(JPEGDIR)/missing
