if HAVE_WIN32
noinst_DATA = main.ico
CLEANFILES = $(noinst_DATA)

main_ico_IMAGES = \
    main-16px-4bpp.png \
    main-32px-4bpp.png \
    main-16px-24bpp.png \
    main-32px-24bpp.png \
    main-48px-24bpp.png \
    main-256px-24bpp.png

EXTRA_DIST = \
    $(main_ico_IMAGES)

main.ico: $(main_ico_IMAGES)
	$(AM_V_GEN)arglist=""; \
	for fn in $^; do \
	    if [ "$${fn/-256px-/-}" != "$$fn" ]; then \
	        arglist="$$arglist -r"; \
	    fi; \
	    arglist="$$arglist $$fn"; \
	done; \
	$(ICOTOOL) -c -o $@ $$arglist || exit 1;
endif
