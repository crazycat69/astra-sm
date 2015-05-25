AM_CFLAGS = $(WARN_CFLAGS) $(LUA_CFLAGS)

noinst_LTLIBRARIES =
bin_PROGRAMS =

#
# Astra library
#
noinst_LTLIBRARIES += libastra.la
libastra_la_CFLAGS = $(AM_CFLAGS)
libastra_la_LDFLAGS = -module -static
libastra_la_LIBADD = $(LIBRT) $(LUA_LIBS)
libastra_la_SOURCES =

# core/
libastra_la_SOURCES += \
    core/clock.c \
    core/compat.c \
    core/event.c \
    core/list.c \
    core/log.c \
    core/loopctl.c \
    core/luaglue.c \
    core/socket.c \
    core/strbuffer.c \
    core/stream.c \
    core/thread.c \
    core/timer.c

# mpegts/
libastra_la_SOURCES += \
    mpegts/descriptors.c \
    mpegts/pcr.c \
    mpegts/psi.c \
    mpegts/pes.c \
    mpegts/sync.c \
    mpegts/t2mi.c \
    mpegts/types.c

# utils/
libastra_la_SOURCES += \
    utils/astra.c \
    utils/base64.c \
    utils/crc32b.c \
    utils/crc8.c \
    utils/iso8859.c \
    utils/json.c \
    utils/log.c \
    utils/md5.c \
    utils/rc4.c \
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
    stream/dvb/ca.c \
    stream/dvb/frontend.c \
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
    stream/softcam/cas/dgcrypt.c \
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

bindings.h:
	$(AM_V_GEN)echo "/* automatically generated file; do not edit */" >$@; \
	echo                        >>$@; \
	echo "/*"                   >>$@; \
	echo " * Core library"      >>$@; \
	echo " */"                  >>$@; \
	echo                        >>$@; \
	sh ../build-aux/gen-bindings-list LUA_CORE_BINDINGS $(libastra_la_SOURCES) >>$@; \
	echo                        >>$@; \
	echo "/*"                   >>$@; \
	echo " * Streaming modules" >>$@; \
	echo " */"                  >>$@; \
	echo                        >>$@; \
	sh ../build-aux/gen-bindings-list LUA_STREAM_BINDINGS $(libstream_la_SOURCES) >>$@

# built-in script
if HAVE_INSCRIPT
INSCRIPT_FILES = \
    $(top_srcdir)/scripts/analyze.luac \
    $(top_srcdir)/scripts/base.luac \
    $(top_srcdir)/scripts/dvbls.luac \
    $(top_srcdir)/scripts/dvbwrite.luac \
    $(top_srcdir)/scripts/femon.luac \
    $(top_srcdir)/scripts/relay.luac \
    $(top_srcdir)/scripts/stream.luac

MKSCRIPT = $(top_srcdir)/tools/mkscript$(BUILD_EXEEXT)
BUILT_SOURCES += inscript/inscript.h

inscript/inscript.h: $(INSCRIPT_FILES)
	$(AM_V_GEN)echo "/* automatically generated file; do not edit */" >$@; \
	echo >>$@; \
	for script in $(INSCRIPT_FILES); do \
	    $(MKSCRIPT) `basename $$script .luac` "$$script" >>$@; \
	done
endif

# preprocessed source code
if HAVE_PREPROCESSED
PP_CFLAGS = $(DEFS) \
    $(DEFAULT_INCLUDES) $(INCLUDES) \
    $(AM_CPPFLAGS) $(CPPFLAGS) \
    $(libastra_la_CFLAGS) \
    $(libstream_la_CFLAGS) \
    $(astra_CFLAGS) \
    $(CFLAGS)

BUILT_SOURCES += $(SOURCES:.c=.i)

SUFFIXES: .i
.c.i: $*.c
	$(AM_V_GEN)$(CPP) $(PP_CFLAGS) -o $@ $<
endif

CLEANFILES = $(BUILT_SOURCES)
