if HAVE_INTERNAL_LUA
noinst_LTLIBRARIES = liblua.la
liblua_la_CFLAGS = -DLUA_COMPAT_ALL
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

# windows
if HAVE_WIN32
liblua_la_CFLAGS += -DLUA_BUILD_AS_DLL
endif

# macos
if HAVE_MACOS
liblua_la_CFLAGS += -DLUA_USE_MACOSX
endif

# freebsd
if HAVE_FREEBSD
liblua_la_CFLAGS += -DLUA_USE_LINUX
endif

# linux
if HAVE_LINUX
liblua_la_CFLAGS += -DLUA_USE_LINUX
endif
endif
