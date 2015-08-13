if HAVE_INTERNAL_LUA
#
# Platform-specific flags
#
AM_CFLAGS = -DLUA_COMPAT_ALL -DLUA_USE_LONGJMP

if HAVE_POSIX
AM_CFLAGS += -DLUA_USE_LINUX
endif

#
# Library
#
noinst_LTLIBRARIES = liblua.la
liblua_la_CFLAGS = $(AM_CFLAGS)
liblua_la_LDFLAGS = -module -static
liblua_la_LIBADD = $(LIBM) $(LIBADD_DLOPEN)
liblua_la_SOURCES = \
    lapi.c \
    lauxlib.c \
    lcode.c \
    lctype.c \
    ldebug.c \
    ldo.c \
    ldump.c \
    lfunc.c \
    lgc.c \
    llex.c \
    lmem.c \
    loadlib.c \
    lobject.c \
    lopcodes.c \
    lparser.c \
    lstate.c \
    lstring.c \
    ltable.c \
    ltm.c \
    lundump.c \
    lvm.c \
    lzio.c \
    lbaselib.c \
    lbitlib.c \
    lcorolib.c \
    ldblib.c \
    linit.c \
    liolib.c \
    lmathlib.c \
    loslib.c \
    lstrlib.c \
    ltablib.c

#
# Compiler
#
if HAVE_LUA_COMPILER
noinst_PROGRAMS = luac
luac_CFLAGS = $(AM_CFLAGS)
luac_LDADD = liblua.la
luac_SOURCES = luac.c
endif
endif
