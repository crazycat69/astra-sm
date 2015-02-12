SCRIPT_FILES = analyze.lua base.lua dvbls.lua dvbwrite.lua relay.lua stream.lua
scriptdir = $(sysconfdir)/astra/scripts

if HAVE_INSCRIPT
SUFFIXES: .luac
BUILT_SOURCES = $(SCRIPT_FILES:.lua=.luac)
CLEANFILES = $(BUILT_SOURCES)

if HAVE_LUA_COMPILER
.lua.luac: $*.lua
	$(AM_V_GEN)$(LUAC) -s -o $@ -- $<
else
.lua.luac:
	$(AM_V_GEN)$(LN_S) -f $< $@
endif
else
script_DATA = $(SCRIPT_FILES)
endif

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(scriptdir)
