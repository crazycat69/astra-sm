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
check_PROGRAMS = unit_tests
TESTS = $(check_PROGRAMS)
AM_COLOR_TESTS = no

unit_tests_LDADD = $(AM_LDADD)
unit_tests_SOURCES = \
    unit_tests.c \
    core_clock.c \
    core_list.c \
    core_mainloop.c \
    core_timer.c
endif

#
# Test programs
#
noinst_PROGRAMS = t2mi_decap spammer

t2mi_decap_SOURCES = t2mi_decap.c
t2mi_decap_CFLAGS = $(AM_CFLAGS)
t2mi_decap_LDADD = $(AM_LDADD)

spammer_SOURCES = spammer.c
spammer_CFLAGS = $(AM_CFLAGS)
spammer_LDADD = $(AM_LDADD)
