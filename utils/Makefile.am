noinst_LTLIBRARIES = utils.la
utils_la_SOURCES = astra.c base64.c crc32b.c iso8859.c json.c log.c md5.c sha1.c strhex.c timer.c utils.c
utils_la_LDFLAGS = -module -static

if !HAVE_WIN32
    utils_la_SOURCES += pidfile.c
endif
