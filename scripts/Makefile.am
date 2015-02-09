if HAVE_INSCRIPT
SUFFIXES: .luac
BUILT_SOURCES = analyze.luac base.luac dvbls.luac dvbwrite.luac relay.luac stream.luac
CLEANFILES = $(BUILT_SOURCES)

if HAVE_LUA_COMPILER
.lua.luac: $*.lua
	@echo -n "Building $@ from $<... "
	@$(LUAC) -s -o "$@" -- "$<"
	@echo "done!"
else
.lua.luac:
	@echo -n "Linking $@ to $<... "
	@$(LN_S) -f "$<" "$@"
	@echo "done!"
endif

else
# TODO: install scripts in $(sysconfdir)/astra/scripts
endif
