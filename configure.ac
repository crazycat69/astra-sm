#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# Get version from git
m4_define([pkg_version], m4_esyscmd_s([build-aux/git-version-gen]))

#
# Autotools setup
#
AC_PREREQ([2.69])
AC_INIT([Astra SM], [pkg_version])
AC_USE_SYSTEM_EXTENSIONS

AC_CONFIG_SRCDIR([src/astra.h])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([build-aux/m4])
AC_CONFIG_LIBOBJ_DIR([src/compat])
AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE([1.11 foreign dist-xz subdir-objects -Wall -Werror])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])
AC_PRESERVE_HELP_ORDER

# Checks for programs
AC_LANG(C)
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_INSTALL
AM_PROG_AR

# Initialize libtool
LT_INIT([disable-shared static no-pic])
LT_LIB_DLLOAD
LT_LIB_M

#
# Checks for typedefs, structures, and compiler characteristics
#
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_INT64_T
AC_CHECK_TYPES([ptrdiff_t])
AC_CHECK_HEADER_STDBOOL

# in case endian.h is absent
AC_CHECK_HEADER([endian.h], [ac_cv_have_endian_h="yes"], [ac_cv_have_endian_h="no"])
if test "x$ac_cv_have_endian_h" = "xno"; then
    AC_DEFINE(__LITTLE_ENDIAN, 1234, [Little endian byte order])
    AC_DEFINE(__BIG_ENDIAN, 4321, [Big endian byte order])
    AC_C_BIGENDIAN([ac_cv_c_bigendian="yes"], [ac_cv_c_bigendian="no"], [], [])

    if test "x$ac_cv_c_bigendian" = "xyes"; then
        AC_DEFINE(__BYTE_ORDER, __BIG_ENDIAN, [Define this machine's endianness])
    else
        AC_DEFINE(__BYTE_ORDER, __LITTLE_ENDIAN, [Define this machine's endianness])
    fi
fi

# use 64 bit file offsets on 32 bit systems
AC_SYS_LARGEFILE

#
# Platform-dependent options
#
case "${host_os}" in
    mingw32*)
        SYS=windows
        AX_CHECK_HEADERS_REQ([windows.h winsock2.h ws2tcpip.h])

        AC_DEFINE(WIN32_LEAN_AND_MEAN,
            [], [Define to limit the scope of <windows.h>.])

        AH_TOP([#if defined(_WIN32) && !defined(_WIN32_WINNT)])
        AH_TOP([#   define _WIN32_WINNT 0x0502 /* Windows XP SP2 */])
        AH_TOP([#endif])

        # check winsock
        AC_CHECK_LIB(ws2_32, main,
            [], AC_MSG_ERROR([cannot link with ws2_32.dll]) )

        # winapi exclusive
        AX_CHECK_WINFUNC(CloseHandle, [ CloseHandle(0) ])
        AX_CHECK_WINFUNC(CreateThread, [ CreateThread(0, 0, 0, 0, 0, 0) ])
        AX_CHECK_WINFUNC(LocalFree, [ LocalFree(0) ])
        AX_CHECK_WINFUNC(SetConsoleCtrlHandler, [ SetConsoleCtrlHandler(0, 0) ])
        AX_CHECK_WINFUNC(WaitForSingleObject, [ WaitForSingleObject(0, 0) ])
        AX_CHECK_WINFUNC(WSAStartup, [ WSAStartup(0, 0) ])
        AX_CHECK_WINFUNC(WSAGetLastError, [ WSAGetLastError() ])
        AX_CHECK_WINFUNC(WSACleanup, [ WSACleanup() ])

        # networking
        AX_CHECK_WINFUNC(accept, [ accept(0, &sa, &sl) ])
        AX_CHECK_WINFUNC(bind, [ bind(0, &sa, sl) ])
        AX_CHECK_WINFUNC(connect, [ connect(0, &sa, sl) ])
        AX_CHECK_WINFUNC(freeaddrinfo, [ freeaddrinfo(&ai) ])
        AX_CHECK_WINFUNC(getaddrinfo, [ getaddrinfo(NULL, NULL, &ai, &pai) ])
        AX_CHECK_WINFUNC(gethostname, [ gethostname(0, 0) ])
        AX_CHECK_WINFUNC(getnameinfo, [ getnameinfo(&sa, sl, NULL, 0, NULL, 0, 0) ])
        AX_CHECK_WINFUNC(getsockname, [ getsockname(0, &sa, &sl) ])
        AX_CHECK_WINFUNC(htons, [ htons(0) ])
        AX_CHECK_WINFUNC(inet_ntoa, [ inet_ntoa(in) ])
        AX_CHECK_WINFUNC(ioctlsocket, [ ioctlsocket(0, 0, 0) ])

        AX_CHECK_WINFUNC(listen, [ listen(0, 0) ])
        AX_CHECK_WINFUNC(recv, [ recv(0, NULL, 0, 0) ])
        AX_CHECK_WINFUNC(recvfrom, [ recvfrom(0, NULL, 0, 0, &sa, &sl) ])
        AX_CHECK_WINFUNC(send, [ send(0, NULL, 0, 0) ])
        AX_CHECK_WINFUNC(sendto, [ sendto(0, NULL, 0, 0, &sa, sl) ])
        AX_CHECK_WINFUNC(setsockopt, [ setsockopt(0, 0, 0, NULL, sl) ])
        AX_CHECK_WINFUNC(shutdown, [ shutdown(0, 0) ])
        AX_CHECK_WINFUNC(socket, [ socket(0, 0, 0) ])

        # standard C
        AX_CHECK_WINFUNC(setjmp, [ setjmp(NULL) ])
    ;;

    linux*)
        SYS=linux
    ;;

    *)
        SYS=unknown
    ;;
esac

if test "${SYS}" != "windows"; then
    AX_CHECK_HEADERS_REQ([syslog.h sys/socket.h arpa/inet.h netinet/in.h netinet/tcp.h netdb.h ifaddrs.h fcntl.h sys/ioctl.h])
    AX_CHECK_FUNCS_REQ([signal syslog closelog mkstemp link fchmod access unlink gethostname setjmp getnameinfo inet_ntoa socket setsockopt bind listen accept getaddrinfo freeaddrinfo connect recv recvfrom send sendto getsockname htons shutdown fcntl ioctl localtime_r])

    # pthread
    AX_PTHREAD([
        LIBS="$PTHREAD_LIBS $LIBS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
        CC="$PTHREAD_CC"
    ], [ AC_MSG_ERROR([missing pthread support]) ])
fi

AM_CONDITIONAL(HAVE_LINUX, [test "${SYS}" = "linux"])
AM_CONDITIONAL(HAVE_WIN32, [test "${SYS}" = "windows"])

# Choose event mechanism
case "${host_os}" in
    mingw32*)
        AC_MSG_NOTICE([using select() for event notification])
        AX_CHECK_WINFUNC(select, [ select(0, NULL, NULL, NULL, NULL) ])
        AC_DEFINE([WITH_SELECT],
            [1], [Define to use select() for event notification])
    ;;

    freebsd*|darwin*)
        AC_MSG_NOTICE([using kqueue for event notification])
        AX_CHECK_HEADERS_REQ([sys/event.h])
        AX_CHECK_FUNCS_REQ([kqueue kevent])
        AC_DEFINE([WITH_KQUEUE],
            [1], [Define to use kqueue for event notification])
    ;;

    linux*)
        AC_MSG_NOTICE([using epoll for event notification])
        AX_CHECK_HEADERS_REQ([sys/epoll.h])
        AX_CHECK_FUNCS_REQ([epoll_create epoll_ctl epoll_wait])
        AC_DEFINE([WITH_EPOLL],
            [1], [Define to use epoll for event notification])
    ;;

    *)
        AC_MSG_NOTICE([using poll() for event notification])
        AX_CHECK_HEADERS_REQ([poll.h])
        AX_CHECK_FUNCS_REQ([poll])
        AC_DEFINE([WITH_POLL],
            [1], [Define to use poll() for event notification])
    ;;
esac

#
# Checks for headers and functions common to all platforms
#
AX_CHECK_HEADERS_REQ([dirent.h stdarg.h stddef.h stdint.h stdbool.h stdio.h stdlib.h sys/types.h sys/stat.h sys/time.h setjmp.h time.h string.h unistd.h errno.h])
AX_CHECK_FUNCS_REQ([localtime longjmp malloc realloc memset memcpy srand strcasecmp strchr strdup strerror strftime strncasecmp strpbrk strrchr strspn strstr strtoul time snprintf vsnprintf memmove])

# optional headers
AC_CHECK_HEADERS([netinet/sctp.h])

# replaceable functions
AC_REPLACE_FUNCS([strndup pread])

# posix_memalign(): used by `file'
AC_CHECK_FUNCS([posix_memalign])

# getifaddrs(): used by utils.c
AC_CHECK_FUNCS([getifaddrs],
    [], [ AC_MSG_WARN([no getifaddrs(); utils.ifaddrs() will be unavailable]) ])

# clock_gettime(): used by clock.c
LIBRT=""
ac_cv_func_clock_gettime="no"
if test "${SYS}" != "windows"; then
    # using clock_gettime() on windows drags winpthreads along
    AC_CHECK_LIB(rt, clock_gettime,
        [
            LIBRT="-lrt"
            ac_cv_func_clock_gettime="yes"
            AC_DEFINE([HAVE_CLOCK_GETTIME],
                [1], [Define to 1 if you have the `clock_gettime' function.])
        ], [
            # try without -lrt
            AC_CHECK_FUNCS([clock_gettime])
        ])
fi
AC_CHECK_FUNCS([gettimeofday])
AS_IF(
    [ test "x${ac_cv_func_clock_gettime}" != "xyes" \
      && test "x${ac_cv_func_gettimeofday}" != "xyes" ],
    [ AC_MSG_ERROR([no function to get time]) ])

# POSIX AIO: used by `file'
have_aio="no"
AC_CHECK_HEADERS([aio.h], [
    AC_CHECK_LIB(rt, aio_init, [
        LIBRT="-lrt"
        have_aio="yes"
    ], [
        # try without -lrt
        AC_CHECK_FUNCS([aio_init], [have_aio="yes"])
    ]) ])

AS_IF([test "x${have_aio}" = "xyes"], [
    AC_DEFINE([HAVE_AIO], [1], [Define to 1 if you have the POSIX AIO interface.]) ])

AC_SUBST(LIBRT)

#
# Checks for external libraries
#
AX_HELP_SECTION([External libraries:])

# lua
AX_EXTLIB_REQUIRED(lua)
AX_EXTLIB_VARS(lua)

have_lua="no"
AX_SAVE_FLAGS
CFLAGS="$CFLAGS $LUA_CFLAGS"
# lua wants -lm on linux
LIBS="$LIBS $LIBM $LUA_LIBS"
AC_CHECK_HEADERS([lua.h], [
    AC_CHECK_LIB([lua], [luaL_len], [
        have_lua="yes"
        LUA_LIBS="${LUA_LIBS} ${LIBM} ${LIBADD_DLOPEN} -llua"
    ])
])
AX_RESTORE_FLAGS
AS_IF([test "x${have_lua}" = "xno"], [
    AC_MSG_ERROR([Astra requires Lua library version 5.2 or higher]) ])

# libaio
AX_EXTLIB_PARAM(libaio,
    [enable libaio for file I/O (Linux only)] )

have_libaio="no"
if test "x${have_aio}" = "xyes" -a "${SYS}" = "linux"; then
    AS_IF([test "x${enable_libaio}" != "xno"], [
        AX_SAVE_FLAGS
        CFLAGS="$CFLAGS $LIBAIO_CFLAGS"
        LIBS="$LIBS $LIBAIO_LIBS"
        AC_CHECK_HEADERS([libaio.h], [
            AC_CHECK_LIB([aio], [io_queue_init], [
                have_libaio="yes"
                LIBAIO_LIBS="${LIBAIO_LIBS} -laio"
            ])
        ])
        AX_RESTORE_FLAGS
        AS_IF([test "x${have_libaio}" = "xno"], [
            AS_IF([test "x${enable_libaio}" = "xyes"],
                [ AC_MSG_ERROR([could not find libaio; pass --disable-libaio to disable this check]) ],
                [ AC_MSG_WARN([could not find libaio; Linux AIO support will be unavailable]) ]) ])
    ])
else
    AS_IF([test "x${enable_libaio}" = "xyes"], [
        AC_MSG_ERROR([libaio is not supported on this platform]) ])
fi
AM_CONDITIONAL(HAVE_LIBAIO, [test "x${have_libaio}" = "xyes"])

# libdvbcsa
AX_EXTLIB_PARAM(dvbcsa,
    [enable CSA (de)scrambling using libdvbcsa] )

have_dvbcsa="no"
AS_IF([test "x${enable_dvbcsa}" != "xno"], [
    AX_SAVE_FLAGS
    CFLAGS="$CFLAGS $DVBCSA_CFLAGS"
    LIBS="$LIBS $DVBCSA_LIBS"
    AC_CHECK_HEADERS([dvbcsa/dvbcsa.h], [
        AC_CHECK_LIB([dvbcsa], [dvbcsa_bs_key_alloc], [
            have_dvbcsa="yes"
            DVBCSA_LIBS="${DVBCSA_LIBS} -ldvbcsa"
        ])
    ])
    AX_RESTORE_FLAGS
    AS_IF([test "x${have_dvbcsa}" = "xno"], [
        AS_IF([test "x${enable_dvbcsa}" = "xyes"],
            [ AC_MSG_ERROR([could not find dvbcsa; pass --disable-dvbcsa to disable this check]) ],
            [ AC_MSG_WARN([could not find dvbcsa; CSA support will be unavailable]) ]) ])
])
AM_CONDITIONAL(HAVE_DVBCSA, [test "x${have_dvbcsa}" = "xyes"])

# OpenSSL (libcrypto)
AC_ARG_WITH(libcrypto, AC_HELP_STRING([--with-libcrypto],
    [enable linking with OpenSSL crypto library (auto)]) )

have_libcrypto="no"
AS_IF([test "x${with_libcrypto}" != "xno"], [
    AX_SAVE_FLAGS
    PKG_CHECK_MODULES([LIBCRYPTO], [libcrypto], [
        AC_CHECK_HEADERS([openssl/des.h], [
            AC_CHECK_LIB([crypto], [DES_random_key], [
                have_libcrypto="yes"
            ], [
                # linking check failed
                LIBCRYPTO_PKG_ERRORS="could not link with libcrypto"
            ])
        ], [
            # header check failed
            LIBCRYPTO_PKG_ERRORS="could not find libcrypto headers"
        ])
    ], [
        # pkgconfig call failed
        LIBCRYPTO_PKG_ERRORS="could not find libcrypto; refer to config.log for details"
    ])
    AX_RESTORE_FLAGS
    AS_IF([test "x${have_libcrypto}" = "xno"], [
        AS_IF([test "x${with_libcrypto}" = "xyes"],
            [ AC_MSG_ERROR([${LIBCRYPTO_PKG_ERRORS}]) ],
            [ AC_MSG_WARN([${LIBCRYPTO_PKG_ERRORS}]) ]) ])
])
AM_CONDITIONAL([HAVE_LIBCRYPTO], [test "x${have_libcrypto}" = "xyes"])

# Linux DVB API
have_dvbapi="no"
AC_MSG_CHECKING([for Linux DVB API version 5 or higher])
AC_PREPROC_IFELSE([
    AC_LANG_PROGRAM([
        #include <sys/ioctl.h>
        #include <linux/dvb/version.h>
        #include <linux/dvb/frontend.h>
        #include <linux/dvb/dmx.h>
        #include <linux/dvb/ca.h>
        #if DVB_API_VERSION < 5
        #error "DVB API is too old"
        #endif
    ])
], [
    AC_MSG_RESULT([yes])
    have_dvbapi="yes"
], [
    AC_MSG_RESULT([no])
])
AS_IF([test "x${have_dvbapi}" = "xno"], [
    AC_MSG_WARN([no DVB API found; DVB functionality will be unavailable]) ])

AM_CONDITIONAL([HAVE_DVBAPI], [test "x${have_dvbapi}" = "xyes"])
AC_DEFINE([HAVE_DVBAPI], [1],
    [Define to 1 if you have a usable DVB API])

#
# Optional features
#
AX_HELP_SECTION([Features and modules:])

# Built-in scripts
AC_ARG_ENABLE(inscript,
    AC_HELP_STRING(--disable-inscript, [don't build Lua scripts into binary (enabled)]))

AS_IF([test "x${enable_inscript}" != "xno"], [
    # TODO: kill this abomination
    AC_CONFIG_FILES([src/inscript/inscript.h],
        [./src/inscript/prepare.sh]) ])

AM_CONDITIONAL([HAVE_INSCRIPT], [test "x${enable_inscript}" != "xno"])

# Streaming modules
stream_modules="analyze channel transmit"

# asi
AX_STREAM_MODULE(asi, [DVB-ASI input module],
    [test "${SYS}" = "linux"], [this module requires Linux])

# biss_encrypt
AX_STREAM_MODULE(biss, [BISS software scrambler],
    [test "x${have_dvbcsa}" = "xyes"], [libdvbcsa is unavailable])

# dvb
AX_STREAM_MODULE(dvb, [DVB input module],
    [test "x${have_dvbapi}" = "xyes"], [Linux DVB API is unavailable])

# file
AX_STREAM_MODULE(file, [file I/O module])

# http
AX_STREAM_MODULE(http, [HTTP server and client])

# remux
AX_STREAM_MODULE(remux, [remultiplexer module])

# softcam
AX_STREAM_MODULE(softcam, [software descrambler],
    [test "x${have_dvbcsa}" = "xyes"], [libdvbcsa is unavailable])

# udp
AX_STREAM_MODULE(udp, [UDP receive and transmit module])

#
# Build options
#
AX_HELP_SECTION([Build options:])

SAVE_CFLAGS="$CFLAGS"
CFLAGS="-std=iso9899:1999"

# Debug version
AC_ARG_ENABLE(debug,
    AC_HELP_STRING(--enable-debug, [build a debug version]))

if test "$enable_debug" = "yes"; then
    AC_DEFINE([DEBUG], [1], [Define to build a debug version])
    SAVE_CFLAGS="$SAVE_CFLAGS -O0 -g3"
else
    CFLAGS="$CFLAGS -fomit-frame-pointer"
fi

# -Werror build
AC_ARG_ENABLE(werror,
    AC_HELP_STRING(--enable-werror, [turn on -Werror (GCC only)]))

# Add generic CFLAGS
CFLAGS="$CFLAGS -fno-builtin -fstrict-aliasing -fstrict-overflow"
if test "$GCC" = "yes"; then
    CFLAGS="$CFLAGS -Waggregate-return -Wall -Wbad-function-cast -Wcast-align -Wextra -Wfloat-equal -Wformat=2 -Winit-self -Winline -Wjump-misses-init -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wmissing-prototypes -Wnested-externs -Wold-style-definition -Wpacked -Wpointer-arith -Wredundant-decls -Wshadow -Wstack-protector -Wstrict-aliasing -Wstrict-overflow=4 -Wstrict-prototypes -Wundef -Wuninitialized -Wunreachable-code -Wunused -Wwrite-strings"
    if test "$enable_werror" = "yes"; then
        CFLAGS="$CFLAGS -pedantic-errors -Werror"
    else
        CFLAGS="$CFLAGS -pedantic"
    fi
fi
CFLAGS="$CFLAGS $SAVE_CFLAGS"

# Makefile generation
AC_CONFIG_FILES([
    Makefile
    src/Makefile
    testcards/Makefile
    scripts/Makefile
])
AC_OUTPUT
