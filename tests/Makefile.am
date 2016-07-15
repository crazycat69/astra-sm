AM_CFLAGS = \
    $(DEFS) $(WARN_CFLAGS) \
    $(CHECK_CFLAGS) $(LUA_CFLAGS) \
    -I$(top_builddir)/src \
    -I$(top_srcdir)/src

AM_LDADD = \
    $(CHECK_LIBS) $(LUA_LIBS) \
    $(top_builddir)/src/libastra.la

#
# Unit tests
#
if HAVE_CHECK
# TODO: fix output redirection on Windows
if HAVE_POSIX
AM_TESTS_ENVIRONMENT = export OUTPUT_REDIRECT_FD=9;
AM_TESTS_FD_REDIRECT = 9>&2
endif

check_PROGRAMS = test_libastra misc/slave
TESTS = test_libastra
CLEANFILES = libastra.log

test_libastra_LDADD = $(AM_LDADD)
test_libastra_DEPENDENCIES = misc/slave$(EXEEXT)
test_libastra_SOURCES = \
    test_libastra.c \
    test_libastra.h

test_libastra_SOURCES += \
    core/alloc.c \
    core/child.c \
    core/clock.c \
    core/list.c \
    core/mainloop.c \
    core/spawn.c \
    core/thread.c \
    core/timer.c

test_libastra_SOURCES += \
    luaapi/luaapi.c \
    luaapi/module.c \
    luaapi/stream.c

test_libastra_SOURCES += \
    luaapi/lib/astra.c \
    luaapi/lib/base64.c \
    luaapi/lib/md5.c \
    luaapi/lib/pidfile.c \
    luaapi/lib/rc4.c \
    luaapi/lib/sha1.c \
    luaapi/lib/strhex.c \
    luaapi/lib/utils.c

test_libastra_SOURCES += \
    utils/base64.c \
    utils/crc32b.c \
    utils/crc8.c \
    utils/md5.c \
    utils/rc4.c \
    utils/sha1.c \
    utils/strhex.c

misc_slave_SOURCES = misc/slave.c
misc_slave_CFLAGS = $(AM_CFLAGS)
endif

#
# Test programs
#
noinst_PROGRAMS = misc/t2mi_decap misc/spammer

misc_t2mi_decap_SOURCES = misc/t2mi_decap.c
misc_t2mi_decap_CFLAGS = $(AM_CFLAGS)
misc_t2mi_decap_LDADD = $(AM_LDADD)

misc_spammer_SOURCES = misc/spammer.c
misc_spammer_CFLAGS = $(AM_CFLAGS)
misc_spammer_LDADD = $(AM_LDADD)
