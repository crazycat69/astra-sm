SCRIPT_FILES = \
    applist.lua \
    autoexec.lua

# common libraries
SCRIPT_FILES += \
    common/base.lua \
    common/stream.lua

# applications
SCRIPT_FILES += \
    apps/analyze.lua \
    apps/dvbls.lua \
    apps/femon.lua \
    apps/relay.lua \
    apps/service.lua \
    apps/stream.lua

# script packaging
script_sysconfdir = $(sysconfdir)/astra/scripts
script_datadir = $(datadir)/astra

install-data-local:
	$(MKDIR_P) \
	    $(DESTDIR)$(script_sysconfdir)/autoexec.d \
	    $(DESTDIR)$(script_datadir)/autoexec.d

if HAVE_INSCRIPT
MKSCRIPT = $(top_builddir)/tools/mkscript$(BUILD_EXEEXT)
BUILT_SOURCES = prepared.h
CLEANFILES = $(BUILT_SOURCES)

if HAVE_LUA_COMPILER
CHKSCRIPT = $(LUAC) -p --
else
CHKSCRIPT = true
endif

prepared.h: $(MKSCRIPT) $(SCRIPT_FILES)
	$(AM_V_GEN)for script in $(SCRIPT_FILES); do \
	    $(CHKSCRIPT) "$(srcdir)/$$script" || exit $$?; \
	done; \
	$(MKSCRIPT) $(srcdir) $(SCRIPT_FILES) >"$@" || \
	    { rc=$$?; rm -f "$@"; exit $$rc; }
else
nobase_dist_script_data_DATA = $(SCRIPT_FILES)
endif
