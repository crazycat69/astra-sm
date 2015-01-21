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
noinst_LTLIBRARIES += libmodules.la
libmodules_la_CFLAGS = $(libastra_la_CFLAGS)
libmodules_la_LDFLAGS = -module -static
libmodules_la_LIBADD = libastra.la
libmodules_la_SOURCES =

### analyze ###
#if HAVE_MODULE_ANALYZE
libmodules_la_SOURCES += \
    modules/analyze/analyze.c
#endif

### asi ###
#if HAVE_MODULE_ASI
libmodules_la_SOURCES += \
    modules/asi/input.c
#endif

### biss_encrypt ###
#if HAVE_MODULE_BISS
libmodules_la_SOURCES += \
    modules/biss_encrypt/biss_encrypt.c
#endif

### channel ###
#if HAVE_MODULE_CHANNEL
libmodules_la_SOURCES += \
    modules/channel/channel.c
#endif

### dvb ###
#if HAVE_MODULE_DVB
libmodules_la_SOURCES += \
    modules/dvb/src/ca.c \
    modules/dvb/src/fe.c \
    modules/dvb/ddci.c \
    modules/dvb/input.c
#endif

### file ###
#if HAVE_MODULE_FILE
libmodules_la_SOURCES += \
    modules/file/input.c \
    modules/file/output.c

if HAVE_LIBAIO
libmodules_la_CFLAGS += $(LIBAIO_CFLAGS)
libmodules_la_LIBADD += $(LIBAIO_LIBS)
endif
#endif

### http ###
#if HAVE_MODULE_HTTP
libmodules_la_SOURCES += \
    modules/http/modules/downstream.c \
    modules/http/modules/redirect.c \
    modules/http/modules/static.c \
    modules/http/modules/upstream.c \
    modules/http/modules/websocket.c \
    modules/http/parser.c \
    modules/http/request.c \
    modules/http/server.c \
    modules/http/utils.c
#endif

### remux ###
#if HAVE_MODULE_REMUX
libmodules_la_SOURCES += \
    modules/remux/remux.c \
    modules/remux/si.c \
    modules/remux/utils.c
#endif

### softcam ###
#if HAVE_MODULE_SOFTCAM
libmodules_la_CFLAGS += $(DVBCSA_CFLAGS)
libmodules_la_LIBADD += $(DVBCSA_LIBS)
libmodules_la_SOURCES += \
    modules/softcam/decrypt.c \
    modules/softcam/cam/cam.c \
    modules/softcam/cas/bulcrypt.c \
    modules/softcam/cas/conax.c \
    modules/softcam/cas/cryptoworks.c \
    modules/softcam/cas/dre.c \
    modules/softcam/cas/exset.c \
    modules/softcam/cas/griffin.c \
    modules/softcam/cas/irdeto.c \
    modules/softcam/cas/mediaguard.c \
    modules/softcam/cas/nagra.c \
    modules/softcam/cas/viaccess.c \
    modules/softcam/cas/videoguard.c

if HAVE_LIBCRYPTO
# need DES for newcamd
libmodules_la_CFLAGS += $(LIBCRYPTO_CFLAGS)
libmodules_la_LIBADD += $(LIBCRYPTO_LIBS)
libmodules_la_SOURCES += \
    modules/softcam/cam/newcamd.c
endif
#endif

### transmit ###
#if HAVE_MODULE_TRANSMIT
libmodules_la_SOURCES += \
    modules/transmit/transmit.c
#endif

### udp ###
#if HAVE_MODULE_UDP
libmodules_la_SOURCES += \
    modules/udp/input.c \
    modules/udp/output.c
#endif

#
# Main binary
#
bin_PROGRAMS += astra
astra_SOURCES = main.c
astra_CFLAGS = \
    $(libastra_la_CFLAGS) \
    $(libmodules_la_CFLAGS)
astra_LDADD = \
    libastra.la \
    libmodules.la

#
# Unit tests
#
bin_PROGRAMS += test
test_LDADD = libastra.la
test_CFLAGS = $(libastra_la_CFLAGS)
test_SOURCES = test.c
