AM_CFLAGS = \
    $(WARN_CFLAGS) \
    -include $(builddir)/config.h \
    -I$(builddir) -I$(srcdir)

# add external library include dirs
AM_CFLAGS += $(LUA_CFLAGS)
if HAVE_CHECK
AM_CFLAGS += $(CHECK_CFLAGS)
endif
if HAVE_DVBCSA
AM_CFLAGS += $(DVBCSA_CFLAGS)
endif
if HAVE_LIBAIO
AM_CFLAGS += $(LIBAIO_CFLAGS)
endif
if HAVE_LIBCRYPTO
AM_CFLAGS += $(LIBCRYPTO_CFLAGS)
endif

# add Lua script path
if HAVE_POSIX
AM_CFLAGS += \
    -DASC_SCRIPTDIR='"$(sysconfdir)/astra/scripts"' \
    -DASC_DATADIR='"$(datadir)/astra"'
endif

# set up windows resource compiler
if HAVE_WIN32
.rc.$(OBJEXT):
	$(AM_V_GEN)$(RC) \
	    -I$(top_builddir)/icons -I$(builddir) \
	    -i $< -J rc -o $@ -O coff
endif

# initialize variables so that '+=' works
BUILT_SOURCES =
CLEANFILES =
bin_PROGRAMS =
noinst_LTLIBRARIES =
noinst_PROGRAMS =

##
## Core library
##
noinst_LTLIBRARIES += libastra.la
libastra_la_LDFLAGS = -module -static
libastra_la_LIBADD = $(LIBRT) $(LUA_LIBS)
libastra_la_SOURCES = \
    astra/astra.h

# core/
libastra_la_SOURCES += \
    astra/core/alloc.h \
    astra/core/assert.h \
    astra/core/child.c \
    astra/core/child.h \
    astra/core/clock.c \
    astra/core/clock.h \
    astra/core/compat.c \
    astra/core/compat.h \
    astra/core/cond.c \
    astra/core/cond.h \
    astra/core/error.c \
    astra/core/error.h \
    astra/core/event.c \
    astra/core/event.h \
    astra/core/event-priv.h \
    astra/core/init.c \
    astra/core/init.h \
    astra/core/list.c \
    astra/core/list.h \
    astra/core/log.c \
    astra/core/log.h \
    astra/core/mainloop.c \
    astra/core/mainloop.h \
    astra/core/mutex.c \
    astra/core/mutex.h \
    astra/core/socket.c \
    astra/core/socket.h \
    astra/core/spawn.c \
    astra/core/spawn.h \
    astra/core/strbuf.c \
    astra/core/strbuf.h \
    astra/core/thread.c \
    astra/core/thread.h \
    astra/core/timer.c \
    astra/core/timer.h

if WITH_SELECT
libastra_la_SOURCES += astra/core/event-select.c
endif
if WITH_POLL
libastra_la_SOURCES += astra/core/event-poll.c
endif
if WITH_KQUEUE
libastra_la_SOURCES += astra/core/event-kqueue.c
endif
if WITH_EPOLL
libastra_la_SOURCES += astra/core/event-epoll.c
endif

# luaapi/
libastra_la_SOURCES += \
    astra/luaapi/luaapi.c \
    astra/luaapi/luaapi.h \
    astra/luaapi/module.c \
    astra/luaapi/module.h \
    astra/luaapi/state.c \
    astra/luaapi/state.h \
    astra/luaapi/stream.c \
    astra/luaapi/stream.h

# luaapi/lib/
libastra_la_SOURCES += \
    astra/luaapi/lib/astra.c \
    astra/luaapi/lib/base64.c \
    astra/luaapi/lib/iso8859.c \
    astra/luaapi/lib/json.c \
    astra/luaapi/lib/log.c \
    astra/luaapi/lib/md5.c \
    astra/luaapi/lib/pidfile.c \
    astra/luaapi/lib/rc4.c \
    astra/luaapi/lib/sha1.c \
    astra/luaapi/lib/strhex.c \
    astra/luaapi/lib/timer.c \
    astra/luaapi/lib/utils.c \
    astra/luaapi/lib/list.h

if HAVE_WIN32
libastra_la_SOURCES += \
    astra/luaapi/lib/winsvc.c
endif

# mpegts/
libastra_la_SOURCES += \
    astra/mpegts/descriptors.c \
    astra/mpegts/descriptors.h \
    astra/mpegts/pcr.c \
    astra/mpegts/pcr.h \
    astra/mpegts/pes.c \
    astra/mpegts/pes.h \
    astra/mpegts/psi.c \
    astra/mpegts/psi.h \
    astra/mpegts/sync.c \
    astra/mpegts/sync.h \
    astra/mpegts/t2mi.c \
    astra/mpegts/t2mi.h \
    astra/mpegts/tscore.h \
    astra/mpegts/types.c \
    astra/mpegts/types.h

# utils/
libastra_la_SOURCES += \
    astra/utils/base64.c \
    astra/utils/base64.h \
    astra/utils/crc32b.c \
    astra/utils/crc32b.h \
    astra/utils/crc8.c \
    astra/utils/crc8.h \
    astra/utils/iso8859.c \
    astra/utils/iso8859.h \
    astra/utils/md5.c \
    astra/utils/md5.h \
    astra/utils/rc4.c \
    astra/utils/rc4.h \
    astra/utils/sha1.c \
    astra/utils/sha1.h \
    astra/utils/strhex.c \
    astra/utils/strhex.h

##
## Streaming library
##
noinst_LTLIBRARIES += libstream.la
libstream_la_LDFLAGS = -module -static
libstream_la_LIBADD = libastra.la
libstream_la_SOURCES = \
    stream/analyze/analyze.c \
    stream/channel/channel.c \
    stream/file/input.c \
    stream/file/output.c \
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
    stream/http/modules/websocket.c \
    stream/pipe/pipe.c \
    stream/remux/remux.c \
    stream/remux/remux.h \
    stream/remux/si.c \
    stream/remux/utils.c \
    stream/t2mi/decap.c \
    stream/transmit/transmit.c \
    stream/udp/input.c \
    stream/udp/output.c

# link external libraries
if HAVE_DVBCSA
libstream_la_LIBADD += $(DVBCSA_LIBS)
if HAVE_LIBCRYPTO
libstream_la_LIBADD += $(LIBCRYPTO_LIBS)
endif
endif
if HAVE_LIBAIO
libstream_la_LIBADD += $(LIBAIO_LIBS)
endif

# CSA scrambling and descrambling
if HAVE_DVBCSA
libstream_la_SOURCES += \
    stream/biss_encrypt/biss_encrypt.c

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
libstream_la_SOURCES += \
    stream/softcam/cam/newcamd.c
endif
endif

# Hardware devices
libstream_la_SOURCES += \
    hwdev/enum/enum.c \
    hwdev/enum/enum.h

# DVB-ASI input
if !HAVE_WIN32
libstream_la_SOURCES += \
    hwdev/asi/input.c
endif

# DVB reception (Linux)
if HAVE_DVBAPI
libstream_la_SOURCES += \
    hwdev/dvbapi/ca.c \
    hwdev/dvbapi/ca.h \
    hwdev/dvbapi/dvb.h \
    hwdev/dvbapi/frontend.c \
    hwdev/dvbapi/frontend.h \
    hwdev/dvbapi/ddci.c \
    hwdev/dvbapi/input.c \
    hwdev/enum/dvbapi.c
endif

# DVB reception (Windows)
if HAVE_WIN32
libstream_la_SOURCES += \
    hwdev/bda/bda.h \
    hwdev/bda/dump.c \
    hwdev/bda/graph.c \
    hwdev/bda/module.c \
    hwdev/bda/networks.c \
    hwdev/bda/vendor.c \
    hwdev/dshow/dshow.c \
    hwdev/dshow/dshow.h \
    hwdev/dshow/grabber.c \
    hwdev/dshow/guids.c \
    hwdev/dshow/guids.h \
    hwdev/enum/bda.c
endif

# dynamically generated symbol lists
DYNAMIC_HDR_LIST = \
    stream/list.h \
    hwdev/enum/list.h

GEN_DYNAMIC_LIST = $(top_srcdir)/build-aux/gen-dynamic-list
$(DYNAMIC_HDR_LIST): $(GEN_DYNAMIC_LIST) config.h
	$(AM_V_GEN)$(GEN_DYNAMIC_LIST) $(srcdir) $@ $(libstream_la_SOURCES) \
	    >$@ || exit $$?

BUILT_SOURCES += $(DYNAMIC_HDR_LIST)
CLEANFILES += $(DYNAMIC_HDR_LIST)

##
## Main binary
##
bin_PROGRAMS += main/astra
main_astra_SOURCES = main/main.c main/sig.h

if HAVE_WIN32
main_astra_LDFLAGS = -municode
main_astra_SOURCES += \
    main/resources.rc \
    main/sig-win32.c
else
main_astra_SOURCES += \
    main/sig-posix.c
endif

if HAVE_INSCRIPT
main_astra_SOURCES += \
    main/inscript.c \
    main/inscript.h
endif

main_astra_LDADD = \
    libastra.la \
    libstream.la

##
## Test programs
##
noinst_PROGRAMS += tests/t2mi_decap
tests_t2mi_decap_SOURCES = tests/t2mi_decap.c
tests_t2mi_decap_LDADD = libastra.la

noinst_PROGRAMS += tests/ts_spammer
tests_ts_spammer_SOURCES = tests/ts_spammer.c

##
## Unit tests
##
if HAVE_CHECK
noinst_PROGRAMS += tests/spawn_slave
tests_spawn_slave_SOURCES = tests/spawn_slave.c

noinst_PROGRAMS += tests/libastra
tests_libastra_SOURCES = \
    tests/libastra.c \
    tests/libastra.h

tests_libastra_SOURCES += \
    tests/core/alloc.c \
    tests/core/assert.c \
    tests/core/child.c \
    tests/core/clock.c \
    tests/core/compat.c \
    tests/core/event.c \
    tests/core/list.c \
    tests/core/mainloop.c \
    tests/core/spawn.c \
    tests/core/thread.c \
    tests/core/timer.c

tests_libastra_SOURCES += \
    tests/luaapi/luaapi.c \
    tests/luaapi/module.c \
    tests/luaapi/state.c \
    tests/luaapi/stream.c

tests_libastra_SOURCES += \
    tests/luaapi/lib/astra.c \
    tests/luaapi/lib/base64.c \
    tests/luaapi/lib/md5.c \
    tests/luaapi/lib/pidfile.c \
    tests/luaapi/lib/rc4.c \
    tests/luaapi/lib/sha1.c \
    tests/luaapi/lib/strhex.c \
    tests/luaapi/lib/utils.c

tests_libastra_SOURCES += \
    tests/utils/base64.c \
    tests/utils/crc32b.c \
    tests/utils/crc8.c \
    tests/utils/md5.c \
    tests/utils/rc4.c \
    tests/utils/rc4_vectors.h \
    tests/utils/sha1.c \
    tests/utils/strhex.c

tests_libastra_LDADD = libastra.la $(CHECK_LIBS)
tests_libastra_DEPENDENCIES = libastra.la \
    tests/spawn_slave$(EXEEXT) \
    tests/ts_spammer$(EXEEXT)

if !CROSS_COMPILING
# TODO: fix output redirection on Windows
if HAVE_POSIX
AM_TESTS_ENVIRONMENT = export OUTPUT_REDIRECT_FD=9;
AM_TESTS_FD_REDIRECT = 9>&2
endif

TESTS = tests/libastra
CLEANFILES += libastra.log
endif
endif
