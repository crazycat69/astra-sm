SUBDIRS = lua src testcards .
DIST_SUBDIRS = $(SUBDIRS) scripts
EXTRA_DIST = build-aux scripts COPYING
ACLOCAL_AMFLAGS = -I build-aux/m4

clean-local:
	find . -name \*.s -exec rm -f {} \; ; \
	find . -name \*.i -exec rm -f {} \;
