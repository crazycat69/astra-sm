SUBDIRS = core modules mpegts utils testcards .
DIST_SUBDIRS = $(SUBDIRS) scripts
EXTRA_DIST = build-aux m4 scripts COPYING
ACLOCAL_AMFLAGS = -I build-aux/m4

bin_PROGRAMS = test
test_SOURCES = test.c
test_LDADD = \
    core/core.la \
    mpegts/mpegts.la \
    utils/utils.la \
    modules/modules.la \
    $(LIBM)
