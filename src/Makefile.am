noinst_LTLIBRARIES =
bin_PROGRAMS =

#
# Astra library
#
noinst_LTLIBRARIES += libastra.la
libastra_la_CFLAGS = $(LUA_CFLAGS)
libastra_la_LDFLAGS = -module -static
libastra_la_LIBADD = $(LIBM) $(LIBRT) $(LUA_LIBS)
libastra_la_SOURCES =

# core/
libastra_la_SOURCES += \
    core/clock.c \
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

if !HAVE_WIN32
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
libstream_la_CFLAGS = $(libastra_la_CFLAGS)
libstream_la_LDFLAGS = -module -static
libstream_la_LIBADD = libastra.la
libstream_la_SOURCES =

### analyze ###
#if HAVE_MODULE_ANALYZE
libstream_la_SOURCES += \
    stream/analyze/analyze.c
#endif

### asi ###
#if HAVE_MODULE_ASI
libstream_la_SOURCES += \
    stream/asi/input.c
#endif

### biss_encrypt ###
#if HAVE_MODULE_BISS
libstream_la_SOURCES += \
    stream/biss_encrypt/biss_encrypt.c
#endif

### channel ###
#if HAVE_MODULE_CHANNEL
libstream_la_SOURCES += \
    stream/channel/channel.c
#endif

### dvb ###
#if HAVE_MODULE_DVB
libstream_la_SOURCES += \
    stream/dvb/src/ca.c \
    stream/dvb/src/fe.c \
    stream/dvb/ddci.c \
    stream/dvb/input.c
#endif

### file ###
#if HAVE_MODULE_FILE
libstream_la_SOURCES += \
    stream/file/input.c \
    stream/file/output.c

if HAVE_LIBAIO
libstream_la_CFLAGS += $(LIBAIO_CFLAGS)
libstream_la_LIBADD += $(LIBAIO_LIBS)
endif
#endif

### http ###
#if HAVE_MODULE_HTTP
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
#endif

### remux ###
#if HAVE_MODULE_REMUX
libstream_la_SOURCES += \
    stream/remux/remux.c \
    stream/remux/si.c \
    stream/remux/utils.c
#endif

### softcam ###
#if HAVE_MODULE_SOFTCAM
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
#endif

### transmit ###
#if HAVE_MODULE_TRANSMIT
libstream_la_SOURCES += \
    stream/transmit/transmit.c
#endif

### udp ###
#if HAVE_MODULE_UDP
libstream_la_SOURCES += \
    stream/udp/input.c \
    stream/udp/output.c
#endif

#
# Main binary
#
bin_PROGRAMS += astra
astra_SOURCES = main.c
astra_CFLAGS = \
    $(libastra_la_CFLAGS) \
    $(libstream_la_CFLAGS)
astra_LDADD = \
    libastra.la \
    libstream.la

#
# Unit tests
#
bin_PROGRAMS += test
test_LDADD = libastra.la
test_CFLAGS = $(libastra_la_CFLAGS)
test_SOURCES = test.c
