if HAVE_INTERNAL_LUA
#
# Platform-specific flags
#
AM_CFLAGS = -DLUA_COMPAT_ALL -DLUA_USE_LONGJMP

if HAVE_POSIX
AM_CFLAGS += -DLUA_USE_LINUX -D_GNU_SOURCE=1
endif

#
# Library
#
noinst_LTLIBRARIES = liblua.la
liblua_la_LDFLAGS = -module -static
liblua_la_LIBADD = $(LIBM) $(LIBADD_DLOPEN)
liblua_la_SOURCES = \
    lapi.c \
    lapi.h \
    lauxlib.c \
    lauxlib.h \
    lbaselib.c \
    lbitlib.c \
    lcode.c \
    lcode.h \
    lcorolib.c \
    lctype.c \
    lctype.h \
    ldblib.c \
    ldebug.c \
    ldebug.h \
    ldo.c \
    ldo.h \
    ldump.c \
    lfunc.c \
    lfunc.h \
    lgc.c \
    lgc.h \
    linit.c \
    liolib.c \
    llex.c \
    llex.h \
    llimits.h \
    lmathlib.c \
    lmem.c \
    lmem.h \
    loadlib.c \
    lobject.c \
    lobject.h \
    lopcodes.c \
    lopcodes.h \
    loslib.c \
    lparser.c \
    lparser.h \
    lstate.c \
    lstate.h \
    lstring.c \
    lstring.h \
    lstrlib.c \
    ltable.c \
    ltable.h \
    ltablib.c \
    ltm.c \
    ltm.h \
    luaconf.h \
    lua.h \
    lualib.h \
    lundump.c \
    lundump.h \
    lvm.c \
    lvm.h \
    lzio.c \
    lzio.h

#
# Compiler
#
if HAVE_LUA_COMPILER
noinst_PROGRAMS = luac
luac_LDADD = liblua.la
luac_SOURCES = luac.c
endif
endif
