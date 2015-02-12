CC = $(CC_FOR_BUILD)
EXEEXT = $(BUILD_EXEEXT)
AM_CFLAGS = $(WARN_CFLAGS)
noinst_PROGRAMS =

if HAVE_INSCRIPT
noinst_PROGRAMS += mkscript
mkscript_SOURCES = mkscript.c
mkscript_CFLAGS = $(LUA_CFLAGS)
endif

if HAVE_FFMPEG
noinst_PROGRAMS += tonegen
tonegen_SOURCES = tonegen.c
tonegen_LDADD = $(LIBM)
endif
