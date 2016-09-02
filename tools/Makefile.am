EXTRA_DIST = mkscript.c tonegen.c
BUILT_SOURCES =

if HAVE_INSCRIPT
BUILT_SOURCES += mkscript$(BUILD_EXEEXT)
mkscript$(BUILD_EXEEXT): mkscript.c
	$(AM_V_GEN)$(CC_FOR_BUILD) $(CFLAGS_FOR_BUILD) $(WARN_CFLAGS) \
	    -o $@ $(srcdir)/mkscript.c
endif

if HAVE_FFMPEG
BUILT_SOURCES += tonegen$(BUILD_EXEEXT)
tonegen$(BUILD_EXEEXT): tonegen.c
	$(AM_V_GEN)$(CC_FOR_BUILD) $(CFLAGS_FOR_BUILD) $(WARN_CFLAGS) $(LIBM_FOR_BUILD) \
	    -o $@ $(srcdir)/tonegen.c
endif

CLEANFILES = $(BUILT_SOURCES)
