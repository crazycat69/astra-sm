AM_CFLAGS = $(DEFS) $(WARN_CFLAGS) $(CHECK_CFLAGS) $(LUA_CFLAGS)
AM_LDADD = \
    ../src/libastra.la \
    ../src/libstream.la \
    $(LUA_LIBS) \
    $(CHECK_LIBS)

#
# Unit tests
#
if HAVE_CHECK
# TODO: actually write those
check_PROGRAMS =
TESTS = $(check_PROGRAMS)
endif

#
# Test programs
#
noinst_PROGRAMS = t2mi_decap
t2mi_decap_SOURCES = t2mi_decap.c
t2mi_decap_CFLAGS = $(AM_CFLAGS)
t2mi_decap_LDADD = $(AM_LDADD)
