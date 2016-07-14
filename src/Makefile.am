AM_CFLAGS = \
    $(WARN_CFLAGS) \
    $(LUA_CFLAGS) \
    -I$(builddir) -I$(srcdir)

EXTRA_DIST =
noinst_LTLIBRARIES =
bin_PROGRAMS =

#
# Astra library
#
noinst_LTLIBRARIES += libastra.la
libastra_la_CFLAGS = $(AM_CFLAGS)
libastra_la_LDFLAGS = -module -static
libastra_la_LIBADD = $(LIBRT) $(LUA_LIBS)
libastra_la_SOURCES = \
    astra.h

# add Lua script path
if HAVE_POSIX
libastra_la_CFLAGS += \
    -DASC_SCRIPT_DIR='"$(sysconfdir)/astra/scripts"'
endif

# core/
libastra_la_SOURCES += \
    core/alloc.h \
    core/assert.h \
    core/child.c \
    core/child.h \
    core/clock.c \
    core/clock.h \
    core/compat.c \
    core/compat.h \
    core/compat_queue.h \
    core/core.h \
    core/error.c \
    core/error.h \
    core/event.c \
    core/event.h \
    core/init.c \
    core/init.h \
    core/list.c \
    core/list.h \
    core/log.c \
    core/log.h \
    core/mainloop.c \
    core/mainloop.h \
    core/mutex.c \
    core/mutex.h \
    core/socket.c \
    core/socket.h \
    core/spawn.c \
    core/spawn.h \
    core/strbuf.c \
    core/strbuf.h \
    core/thread.c \
    core/thread.h \
    core/timer.c \
    core/timer.h

# luaapi/
libastra_la_SOURCES += \
    luaapi/luaapi.c \
    luaapi/luaapi.h \
    luaapi/module.c \
    luaapi/module.h \
    luaapi/state.c \
    luaapi/state.h \
    luaapi/stream.c \
    luaapi/stream.h

# luaapi/lib/
libastra_la_SOURCES += \
    luaapi/lib/astra.c \
    luaapi/lib/base64.c \
    luaapi/lib/iso8859.c \
    luaapi/lib/json.c \
    luaapi/lib/log.c \
    luaapi/lib/md5.c \
    luaapi/lib/pidfile.c \
    luaapi/lib/rc4.c \
    luaapi/lib/sha1.c \
    luaapi/lib/strhex.c \
    luaapi/lib/timer.c \
    luaapi/lib/utils.c \
    luaapi/lib/list.h

# mpegts/
libastra_la_SOURCES += \
    mpegts/descriptors.c \
    mpegts/descriptors.h \
    mpegts/mpegts.h \
    mpegts/pcr.c \
    mpegts/pcr.h \
    mpegts/pes.c \
    mpegts/pes.h \
    mpegts/psi.c \
    mpegts/psi.h \
    mpegts/sync.c \
    mpegts/sync.h \
    mpegts/t2mi.c \
    mpegts/t2mi.h \
    mpegts/tscore.h \
    mpegts/types.c \
    mpegts/types.h

# utils/
libastra_la_SOURCES += \
    utils/base64.c \
    utils/base64.h \
    utils/crc32b.c \
    utils/crc32b.h \
    utils/crc8.c \
    utils/crc8.h \
    utils/iso8859.c \
    utils/iso8859.h \
    utils/md5.c \
    utils/md5.h \
    utils/rc4.c \
    utils/rc4.h \
    utils/sha1.c \
    utils/sha1.h \
    utils/strhex.c \
    utils/strhex.h

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
    stream/transmit/transmit.c \
    stream/t2mi/decap.c

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
EXTRA_DIST += stream/biss_encrypt/README.md

### dvb ###
if HAVE_STREAM_DVB
libstream_la_SOURCES += \
    stream/dvb/ca.c \
    stream/dvb/ca.h \
    stream/dvb/dvb.h \
    stream/dvb/dvbls.c \
    stream/dvb/frontend.c \
    stream/dvb/frontend.h \
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
    stream/http/http.h \
    stream/http/parser.c \
    stream/http/parser.h \
    stream/http/request.c \
    stream/http/server.c \
    stream/http/utils.c \
    stream/http/modules/downstream.c \
    stream/http/modules/redirect.c \
    stream/http/modules/static.c \
    stream/http/modules/upstream.c \
    stream/http/modules/websocket.c
endif

### pipe ###
if HAVE_STREAM_PIPE
libstream_la_SOURCES += \
    stream/pipe/pipe.c
endif

### remux ###
if HAVE_STREAM_REMUX
libstream_la_SOURCES += \
    stream/remux/remux.c \
    stream/remux/remux.h \
    stream/remux/si.c \
    stream/remux/utils.c
endif

### softcam ###
if HAVE_STREAM_SOFTCAM
libstream_la_CFLAGS += $(DVBCSA_CFLAGS)
libstream_la_LIBADD += $(DVBCSA_LIBS)
libstream_la_SOURCES += \
    stream/softcam/decrypt.c \
    stream/softcam/module_cam.h \
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
EXTRA_DIST += stream/softcam/cas/template.c

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
astra_CFLAGS = $(AM_CFLAGS)
astra_LDADD = \
    libastra.la \
    libstream.la
astra_SOURCES = \
    main.c \
    sighandler.c \
    sighandler.h

if HAVE_INSCRIPT
astra_SOURCES += \
    inscript.c
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
	sh $(top_srcdir)/build-aux/gen-bindings-list \
	    LUA_STREAM_BINDINGS \
	    $(srcdir) $(libstream_la_SOURCES) $(astra_SOURCES) >>$@ || exit $$?

# built-in script
if HAVE_INSCRIPT
INSCRIPT_FILES = \
    $(top_builddir)/scripts/analyze.luac \
    $(top_builddir)/scripts/base.luac \
    $(top_builddir)/scripts/dvbls.luac \
    $(top_builddir)/scripts/dvbwrite.luac \
    $(top_builddir)/scripts/femon.luac \
    $(top_builddir)/scripts/relay.luac \
    $(top_builddir)/scripts/stream.luac

MKSCRIPT = $(top_builddir)/tools/mkscript$(BUILD_EXEEXT)
BUILT_SOURCES += inscript.h

inscript.h: $(INSCRIPT_FILES)
	$(AM_V_GEN)echo "/* automatically generated file; do not edit */" >$@; \
	echo >>$@; \
	for script in $(INSCRIPT_FILES); do \
	    $(MKSCRIPT) `basename $$script .luac` "$$script" >>$@ || exit $$?; \
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
