# Makefile.am, was previously Makefile.in (before 2021)
#
# Copyright 2021 Joao Eriberto Mota Filho <eriberto@eriberto.pro.br>
#
# This file is under the same license of the outguess.

AUTOMAKE_OPTIONS = foreign no-dependencies

SUBDIRS = src tests

man_MANS = man/outguess.1 man/seek_script.1

distclean-local:
	rm -rf autom4te.cache \
               conf[a-zA-Z0-9][a-zA-Z0-9][a-zA-Z0-9][a-zA-Z0-9][a-zA-Z0-9][a-zA-Z0-9]
	rm -f  aclocal.m4 compile config.* configure depcomp install-sh \
               Makefile.in missing src/Makefile.in src/jpeg-6b-steg/config.log \
               src/jpeg-6b-steg/config.status test-driver tests/Makefile.in
