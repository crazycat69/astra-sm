#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# Get version from git
m4_define([pkg_version], m4_esyscmd([build-aux/git-version-gen]))
m4_ifset([pkg_version], [], [ m4_define([pkg_version], [UNKNOWN]) ])
m4_if(pkg_version, [UNKNOWN],
    [ m4_fatal([unable to obtain package version; please check `git-version-gen' output]) ])

#
# Autotools setup
#
AC_PREREQ([2.63])
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
m4_pattern_allow([AM_PROG_AR])
AM_PROG_AR

# Initialize libtool
LT_INIT([disable-shared static no-pic])
LT_LIB_DLLOAD
LT_LIB_M

# Check C99 support
AX_CHECK_CFLAG([-std=c99], [], [], [
    AX_CHECK_CFLAG([-std=iso9899:1999], [], [], [
        AC_MSG_ERROR([$CC doesn't seem to support C99]) ]) ])

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
AC_CHECK_TYPES([_Bool])

# in case endian.h is absent
AC_CHECK_HEADER([endian.h],
    [ ac_cv_have_endian_h="yes" ],
    [ ac_cv_have_endian_h="no" ])

AS_IF([test "x${ac_cv_have_endian_h}" = "xno"], [
    AC_DEFINE([__LITTLE_ENDIAN], [1234], [Little endian byte order])
    AC_DEFINE([__BIG_ENDIAN], [4321], [Big endian byte order])
    AC_C_BIGENDIAN([ac_cv_c_bigendian="yes"], [ac_cv_c_bigendian="no"], [], [])

    AS_IF([test "x${ac_cv_c_bigendian}" = "xyes"], [
        AC_DEFINE([__BYTE_ORDER], [__BIG_ENDIAN],
            [Define this machine's endianness])
    ], [
        AC_DEFINE([__BYTE_ORDER], [__LITTLE_ENDIAN],
            [Define this machine's endianness])
    ])
])

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

    darwin*)
        SYS=macos
    ;;

    freebsd*)
        SYS=freebsd
    ;;

    *)
        SYS=unknown
    ;;
esac

AS_IF([test "${SYS}" != "windows"], [
    AX_CHECK_HEADERS_REQ([stdbool.h syslog.h sys/socket.h arpa/inet.h netinet/in.h netinet/tcp.h netdb.h ifaddrs.h fcntl.h sys/ioctl.h])
    AX_CHECK_FUNCS_REQ([signal syslog closelog mkstemp link fchmod access unlink gethostname setjmp getnameinfo inet_ntoa socket setsockopt bind listen accept getaddrinfo freeaddrinfo connect recv recvfrom send sendto getsockname htons shutdown fcntl ioctl localtime_r])

    # pthread
    AX_PTHREAD([
        LIBS="$PTHREAD_LIBS $LIBS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
        CC="$PTHREAD_CC"
    ], [ AC_MSG_ERROR([missing pthread support]) ])
])

AM_CONDITIONAL(HAVE_LINUX, [test "${SYS}" = "linux"])
AM_CONDITIONAL(HAVE_WIN32, [test "${SYS}" = "windows"])
AM_CONDITIONAL(HAVE_MACOS, [test "${SYS}" = "macos"])
AM_CONDITIONAL(HAVE_FREEBSD, [test "${SYS}" = "freebsd"])

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
AX_SAVE_FLAGS
AC_SEARCH_LIBS([clock_gettime], [rt], [
    AS_IF([test "$LIBS" != "$SAVED_LIBS"], [ LIBRT="-lrt" ])

    ac_cv_func_clock_gettime="yes"
    AC_DEFINE([HAVE_CLOCK_GETTIME],
        [1], [Define to 1 if you have the `clock_gettime' function.])
])
AX_RESTORE_FLAGS

AC_CHECK_FUNCS([gettimeofday])
AS_IF(
    [ test "x${ac_cv_func_clock_gettime}" != "xyes" \
      && test "x${ac_cv_func_gettimeofday}" != "xyes" ],
    [ AC_MSG_ERROR([no function to get time]) ])

# POSIX AIO: used by `file'
have_aio="no"
AX_SAVE_FLAGS
AC_CHECK_HEADERS([aio.h], [
    AC_SEARCH_LIBS([aio_init], [rt], [
        AS_IF([test "$LIBS" != "$SAVED_LIBS"], [ LIBRT="-lrt" ])

        have_aio="yes"
        AC_DEFINE([HAVE_AIO], [1],
            [Define to 1 if you have the POSIX AIO interface.])
    ])
])
AX_RESTORE_FLAGS
AC_SUBST(LIBRT)

#
# Checks for external libraries
#
AX_HELP_SECTION([External libraries:])

# lua
AX_EXTLIB_PARAM(lua,
    [use external Lua library instead of the bundled version])

have_lua="no"
AS_IF([test "x${with_lua}" != "xno"], [
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
        AC_MSG_WARN([Astra requires Lua library version 5.2 or higher])
        AS_IF([test "x${with_lua}" != "xyes"], [
            AC_MSG_WARN([Bundled version will be built and statically linked])
        ], [
            AC_MSG_ERROR([Pass --without-lua to build bundled version])
        ])
    ])
])
AM_CONDITIONAL([HAVE_INTERNAL_LUA], [test "x${have_lua}" = "xno"])

# libaio
AX_EXTLIB_PARAM(libaio,
    [enable libaio for file I/O (Linux only)] )

have_libaio="no"
AS_IF([test "x${have_aio}" = "xyes" -a "${SYS}" = "linux"], [
    AS_IF([test "x${with_libaio}" != "xno"], [
        AX_SAVE_FLAGS
        CFLAGS="$CFLAGS $LIBAIO_CFLAGS"
        LIBS="$LIBS $LIBAIO_LIBS"
        AC_CHECK_HEADERS([libaio.h], [
            AC_CHECK_LIB([aio], [io_queue_init], [
                AC_DEFINE([HAVE_LIBAIO], [1], [Define to 1 if you have libaio])

                have_libaio="yes"
                LIBAIO_LIBS="${LIBAIO_LIBS} -laio"
            ])
        ])
        AX_RESTORE_FLAGS
        AS_IF([test "x${have_libaio}" = "xno"], [
            AS_IF([test "x${with_libaio}" = "xyes"],
                [ AC_MSG_ERROR([could not find libaio; pass --disable-libaio to disable this check]) ],
                [ AC_MSG_WARN([could not find libaio; Linux AIO support will be unavailable]) ]) ])
    ])
], [
    AS_IF([test "x${with_libaio}" = "xyes"], [
        AC_MSG_ERROR([libaio is not supported on this platform]) ])
])
AM_CONDITIONAL([HAVE_LIBAIO], [test "x${have_libaio}" = "xyes"])

# libdvbcsa
AX_EXTLIB_PARAM(dvbcsa,
    [enable CSA (de)scrambling using libdvbcsa] )

have_dvbcsa="no"
AS_IF([test "x${with_dvbcsa}" != "xno"], [
    AX_SAVE_FLAGS
    CFLAGS="$CFLAGS $DVBCSA_CFLAGS"
    LIBS="$LIBS $DVBCSA_LIBS"
    AC_CHECK_HEADERS([dvbcsa/dvbcsa.h], [
        AC_CHECK_LIB([dvbcsa], [dvbcsa_bs_key_alloc], [
            AC_DEFINE([HAVE_DVBCSA], [1], [Define to 1 if you have libdvbcsa])

            have_dvbcsa="yes"
            DVBCSA_LIBS="${DVBCSA_LIBS} -ldvbcsa"
        ])
    ])
    AX_RESTORE_FLAGS
    AS_IF([test "x${have_dvbcsa}" = "xno"], [
        AS_IF([test "x${with_dvbcsa}" = "xyes"],
            [ AC_MSG_ERROR([could not find dvbcsa; pass --disable-dvbcsa to disable this check]) ],
            [ AC_MSG_WARN([could not find dvbcsa; CSA support will be unavailable]) ]) ])
])
AM_CONDITIONAL([HAVE_DVBCSA], [test "x${have_dvbcsa}" = "xyes"])

# OpenSSL (libcrypto)
AX_EXTLIB_OPTIONAL(libcrypto,
    [enable linking with OpenSSL crypto library])

AS_IF([test "x${with_libcrypto_includes}" != "xno"],
    [LIBCRYPTO_CFLAGS="-I${with_libcrypto_includes}"])
AS_IF([test "x${with_libcrypto_libs}" != "xno"],
    [LIBCRYPTO_LIBS="-L${with_libcrypto_libs} -lcrypto"])

have_libcrypto="no"
AS_IF([test "x${with_libcrypto}" != "xno"], [
    AX_SAVE_FLAGS
    PKG_CHECK_MODULES([LIBCRYPTO], [libcrypto], [
        AC_CHECK_HEADERS([openssl/des.h], [
            AC_CHECK_LIB([crypto], [DES_random_key], [
                AC_DEFINE([HAVE_LIBCRYPTO], [1],
                    [Define to 1 if you have OpenSSL libcrypto])

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
    AC_DEFINE([HAVE_DVBAPI], [1], [Define to 1 if you have a usable DVB API])

    have_dvbapi="yes"
], [
    AC_MSG_RESULT([no])
])
AS_IF([test "x${have_dvbapi}" = "xno"], [
    AC_MSG_WARN([no DVB API found; DVB functionality will be unavailable]) ])

AM_CONDITIONAL([HAVE_DVBAPI], [test "x${have_dvbapi}" = "xyes"])

#
# Optional features
#
AX_HELP_SECTION([Features and modules:])

# Built-in scripts
AC_ARG_ENABLE([inscript], AC_HELP_STRING([--disable-inscript],
    [don't build Lua scripts into binary (enabled)]))

AS_IF([test "x${enable_inscript}" != "xno"], [
    AC_DEFINE([HAVE_INSCRIPT], [1],
        [Define to 1 if built-in script module is enabled])
])
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

# Debug version
AC_ARG_ENABLE([debug],
    AC_HELP_STRING([--enable-debug], [build a debug version]))

AS_IF([test "x${enable_debug}" = "xyes"], [
    AC_DEFINE([DEBUG], [1], [Define when building a debug version])

    # disable optimization, add debug info
    AX_CHECK_CFLAGS([-O0 -g3], [], [], [
        AC_MSG_ERROR([$CC doesn't support required debug flag; pass --disable-debug to disable this check]) ])

    # non-essential debug flags
    AX_CHECK_CFLAGS([-save-temps=obj -fno-builtin])
], [
    AX_CHECK_CFLAG([-fomit-frame-pointer])
])

# Lots of warnings
AC_ARG_ENABLE([warnings],
    AC_HELP_STRING([--enable-warnings], [turn on additional compiler warnings]))

# these are always on
AX_CHECK_CFLAG([-Wall])
AX_CHECK_CFLAG([-Wextra])

AS_IF([test "x${enable_warnings}" = "xyes"], [
    AX_CHECK_CFLAG([-Waggregate-return])
    AX_CHECK_CFLAG([-Wbad-function-cast])
    AX_CHECK_CFLAG([-Wcast-align])
    AX_CHECK_CFLAG([-Wfloat-equal])
    AX_CHECK_CFLAG([-Wformat=2])
    AX_CHECK_CFLAG([-Winit-self])
    AX_CHECK_CFLAG([-Winline])
    AX_CHECK_CFLAG([-Wmissing-declarations])
    AX_CHECK_CFLAG([-Wmissing-include-dirs])
    AX_CHECK_CFLAG([-Wmissing-prototypes])
    AX_CHECK_CFLAG([-Wnested-externs])
    AX_CHECK_CFLAG([-Wold-style-definition])
    AX_CHECK_CFLAG([-Wpedantic])
    AX_CHECK_CFLAG([-Wpointer-arith])
    AX_CHECK_CFLAG([-Wredundant-decls])
    AX_CHECK_CFLAG([-Wshadow])
    AX_CHECK_CFLAG([-Wstack-protector])
    AX_CHECK_CFLAG([-Wstrict-aliasing])
    AX_CHECK_CFLAG([-Wstrict-overflow=4])
    AX_CHECK_CFLAG([-Wstrict-prototypes])
    AX_CHECK_CFLAG([-Wundef])
    AX_CHECK_CFLAG([-Wuninitialized])
    AX_CHECK_CFLAG([-Wunused])
    AX_CHECK_CFLAG([-Wwrite-strings])

    # GCC-specific
    AX_CHECK_CFLAG([-Wjump-misses-init])
    AX_CHECK_CFLAG([-Wlogical-op])

    # Clang-specific
    AX_CHECK_CFLAG([-Wgnu])
])

# -Werror build
AC_ARG_ENABLE([werror],
    AC_HELP_STRING([--enable-werror], [turn on -Werror]))

AS_IF([test "x${enable_werror}" = "xyes"], [
    AX_CHECK_CFLAGS([-Werror])
])

# Other flags
AX_CHECK_CFLAG([-fstrict-aliasing])
AX_CHECK_CFLAG([-fstrict-overflow])

# Makefile generation
AC_CONFIG_FILES([
    Makefile
    lua/Makefile
    src/Makefile
    testcards/Makefile
    scripts/Makefile
])
AC_OUTPUT
