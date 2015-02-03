AM_CFLAGS = $(WARN_CFLAGS) $(LUA_CFLAGS)

noinst_LTLIBRARIES =
bin_PROGRAMS =

#
# Astra library
#
noinst_LTLIBRARIES += libastra.la
libastra_la_CFLAGS = $(AM_CFLAGS)
libastra_la_LDFLAGS = -module -static
libastra_la_LIBADD = $(LIBM) $(LIBRT) $(LUA_LIBS)
libastra_la_SOURCES =

# core/
libastra_la_SOURCES += \
    core/clock.c \
    core/compat.c \
    core/event.c \
    core/list.c \
    core/log.c \
    core/loopctl.c \
    core/option.c \
    core/socket.c \
    core/strbuffer.c \
    core/stream.c \
    core/thread.c \
    core/timer.c

# mpegts/
libastra_la_SOURCES += \
    mpegts/pcr.c \
    mpegts/psi.c \
    mpegts/pes.c \
    mpegts/types.c

# utils/
libastra_la_SOURCES += \
    utils/astra.c \
    utils/base64.c \
    utils/crc32b.c \
    utils/iso8859.c \
    utils/json.c \
    utils/log.c \
    utils/md5.c \
    utils/sha1.c \
    utils/strhex.c \
    utils/timer.c \
    utils/utils.c

if HAVE_DVBAPI
libastra_la_SOURCES += \
    utils/dvbls.c
endif

if !HAVE_MINGW
# no pid files on Windows
libastra_la_SOURCES += \
    utils/pidfile.c
endif

if HAVE_INSCRIPT
libastra_la_SOURCES += \
    inscript/inscript.c
endif

#
# Module library
#
noinst_LTLIBRARIES += libstream.la
libstream_la_CFLAGS = $(AM_CFLAGS)
libstream_la_LDFLAGS = -module -static
libstream_la_LIBADD = libastra.la
libstream_la_SOURCES = \
    stream/analyze/analyze.c \
    stream/channel/channel.c \
    stream/transmit/transmit.c

### asi ###
if HAVE_STREAM_ASI
libstream_la_SOURCES += \
    stream/asi/input.c
endif

### biss_encrypt ###
if HAVE_STREAM_BISS
libstream_la_SOURCES += \
    stream/biss_encrypt/biss_encrypt.c
endif

### dvb ###
if HAVE_STREAM_DVB
libstream_la_SOURCES += \
    stream/dvb/src/ca.c \
    stream/dvb/src/fe.c \
    stream/dvb/ddci.c \
    stream/dvb/input.c
endif

### file ###
if HAVE_STREAM_FILE
libstream_la_SOURCES += \
    stream/file/input.c \
    stream/file/output.c

if HAVE_LIBAIO
libstream_la_CFLAGS += $(LIBAIO_CFLAGS)
libstream_la_LIBADD += $(LIBAIO_LIBS)
endif
endif

### http ###
if HAVE_STREAM_HTTP
libstream_la_SOURCES += \
    stream/http/modules/downstream.c \
    stream/http/modules/redirect.c \
    stream/http/modules/static.c \
    stream/http/modules/upstream.c \
    stream/http/modules/websocket.c \
    stream/http/parser.c \
    stream/http/request.c \
    stream/http/server.c \
    stream/http/utils.c
endif

### remux ###
if HAVE_STREAM_REMUX
libstream_la_SOURCES += \
    stream/remux/remux.c \
    stream/remux/si.c \
    stream/remux/utils.c
endif

### softcam ###
if HAVE_STREAM_SOFTCAM
libstream_la_CFLAGS += $(DVBCSA_CFLAGS)
libstream_la_LIBADD += $(DVBCSA_LIBS)
libstream_la_SOURCES += \
    stream/softcam/decrypt.c \
    stream/softcam/cam/cam.c \
    stream/softcam/cas/bulcrypt.c \
    stream/softcam/cas/conax.c \
    stream/softcam/cas/cryptoworks.c \
    stream/softcam/cas/dre.c \
    stream/softcam/cas/exset.c \
    stream/softcam/cas/griffin.c \
    stream/softcam/cas/irdeto.c \
    stream/softcam/cas/mediaguard.c \
    stream/softcam/cas/nagra.c \
    stream/softcam/cas/viaccess.c \
    stream/softcam/cas/videoguard.c

if HAVE_LIBCRYPTO
# need DES for newcamd
libstream_la_CFLAGS += $(LIBCRYPTO_CFLAGS)
libstream_la_LIBADD += $(LIBCRYPTO_LIBS)
libstream_la_SOURCES += \
    stream/softcam/cam/newcamd.c
endif
endif

### udp ###
if HAVE_STREAM_UDP
libstream_la_SOURCES += \
    stream/udp/input.c \
    stream/udp/output.c
endif

#
# Main binary
#
bin_PROGRAMS += astra
astra_SOURCES = main.c
astra_CFLAGS = $(AM_CFLAGS)
astra_LDADD = \
    libastra.la \
    libstream.la

# add Lua script path
if !HAVE_MINGW
astra_CFLAGS += \
    -DASC_SPATH='"$(sysconfdir)/astra/scripts"'
endif

#
# Built sources
#
BUILT_SOURCES =

# lua initializer lists
BUILT_SOURCES += bindings.h
bindings.h: $(libastra_la_SOURCES) $(libstream_la_SOURCES)
	@echo -n "Generating bindings.h... "
	@echo "/* automatically generated file; do not edit */" >bindings.h
	
	@echo                        >>bindings.h
	@echo "/*"                   >>bindings.h
	@echo " * Core library"      >>bindings.h
	@echo " */"                  >>bindings.h
	@echo                        >>bindings.h
	@sh ../build-aux/gen-bindings-list LUA_CORE_BINDINGS $(libastra_la_SOURCES) >>bindings.h
	
	@echo                        >>bindings.h
	@echo "/*"                   >>bindings.h
	@echo " * Streaming modules" >>bindings.h
	@echo " */"                  >>bindings.h
	@echo                        >>bindings.h
	@sh ../build-aux/gen-bindings-list LUA_STREAM_BINDINGS $(libstream_la_SOURCES) >>bindings.h
	
	@echo "done!"

### inscript ###
if HAVE_INSCRIPT
INSCRIPT_FILES = \
    $(top_srcdir)/scripts/analyze.lua \
    $(top_srcdir)/scripts/base.lua \
    $(top_srcdir)/scripts/dvbls.lua \
    $(top_srcdir)/scripts/dvbwrite.lua \
    $(top_srcdir)/scripts/relay.lua \
    $(top_srcdir)/scripts/stream.lua

BUILT_SOURCES += mkscript$(EXEEXT)
# we have to build mkscript with host compiler
mkscript$(EXEEXT):
	$(HOST_CC) -std=c99 -o mkscript$(EXEEXT) inscript/mkscript.c

BUILT_SOURCES += inscript/inscript.h
inscript/inscript.h: mkscript$(EXEEXT) $(INSCRIPT_FILES)
	@echo -n "Generating inscript.h... "
	@echo "/* automatically generated file; do not edit */" >inscript/inscript.h
	
	@echo >>inscript/inscript.h
	@for script in $(INSCRIPT_FILES); do \
	    ./mkscript$(EXEEXT) `basename $$script .lua` "$$script" >>inscript/inscript.h; \
	done
	
	@echo "done!"
endif

CLEANFILES = $(BUILT_SOURCES)
