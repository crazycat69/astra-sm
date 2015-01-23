#
# Custom macros for Astra SM
#
AC_DEFUN([AX_HELP_SECTION], [m4_divert_once([HELP_ENABLE], [
$1])])

AC_DEFUN([AX_CHECK_HEADERS_REQ], [
    AC_CHECK_HEADERS([$1], [], [ AC_MSG_ERROR([missing required header file]) ])
])

AC_DEFUN([AX_CHECK_FUNCS_REQ], [
    AC_CHECK_FUNCS([$1], [], [ AC_MSG_ERROR([missing required library function]) ])
])

AC_DEFUN([AX_SAVE_FLAGS], [
    SAVED_CFLAGS="$CFLAGS"
    SAVED_LIBS="$LIBS"
])

AC_DEFUN([AX_RESTORE_FLAGS], [
    CFLAGS="$SAVED_CFLAGS"
    LIBS="$SAVED_LIBS"
])

AC_DEFUN([AX_CHECK_WINFUNC], [
    AX_SAVE_FLAGS
    AC_MSG_CHECKING([for $1])
    AC_LINK_IFELSE([
        AC_LANG_PROGRAM([[
            #include <windows.h>
            #include <winsock2.h>
            #include <ws2tcpip.h>
            #include <string.h>
            #include <setjmp.h>
        ]], [[
            struct in_addr in;
            struct sockaddr sa;
            struct addrinfo ai;
            struct addrinfo *pai = &ai;
            socklen_t sl;
            $2;
        ]])
    ], [
        AC_MSG_RESULT([yes])
    ], [
        AC_MSG_RESULT([no])
        AC_MSG_ERROR([missing required library function])
    ])
    AX_RESTORE_FLAGS
])

AC_DEFUN([AX_EXTLIB_VARS], [
    # makefile variables
    m4_define([$1_ucase],[m4_translit([$1], [a-z], [A-Z])])

    AS_IF([test "x${with_$1_includes}" != "xno"],
        $1_ucase[_CFLAGS="-I${with_$1_includes}"],
        $1_ucase[_CFLAGS=""])
    AS_IF([test "x${with_$1_libs}" != "xno"],
        $1_ucase[_LIBS="-L${with_$1_libs}"],
        $1_ucase[_LIBS=""])

    AC_SUBST($1_ucase[_CFLAGS])
    AC_SUBST($1_ucase[_LIBS])
    m4_undefine([$1_ucase])
])

AC_DEFUN([AX_EXTLIB_OPTIONAL], [
    # optional dependency parameters
    AC_ARG_WITH($1, AC_HELP_STRING([--with-$1], [$2 (auto)]))
    AC_ARG_WITH($1-includes,
        AC_HELP_STRING([--with-$1-includes=PATH], [path to $1 header files]),
        [with_$1="yes"], [with_$1_includes="no"])
    AC_ARG_WITH($1-libs,
        AC_HELP_STRING([--with-$1-libs=PATH], [path to $1 library files]),
        [with_$1="yes"], [with_$1_libs="no"])
])

AC_DEFUN([AX_EXTLIB_REQUIRED], [
    # required dependency parameters
    AC_ARG_WITH($1-includes,
        AC_HELP_STRING([--with-$1-includes=PATH], [path to $1 header files]),
        [], [with_$1_includes="no"])
    AC_ARG_WITH($1-libs,
        AC_HELP_STRING([--with-$1-libs=PATH], [path to $1 library files]),
        [], [with_$1_libs="no"])
])

AC_DEFUN([AX_EXTLIB_PARAM], [
    AX_EXTLIB_OPTIONAL($1, $2)
    AX_EXTLIB_VARS($1)
])

AC_DEFUN([AX_STREAM_MODULE], [
    m4_define([$1_varname], [m4_translit([$1], [a-z], [A-Z])])
    AC_ARG_ENABLE($1,
        AC_HELP_STRING([--disable-$1], [disable $2 (auto)]))

    have_$1="no"
    AC_MSG_CHECKING([whether to build stream module `$1'])
    AS_IF([test "x${enable_$1}" != "xno"], [
        AS_IF([m4_default([$3], [true])], [
            # auto/force on
            AC_MSG_RESULT([yes])
            AC_DEFINE([HAVE_STREAM_]$1_varname, [1],
                [Define to 1 if the `$1' module is enabled])

            stream_modules="${stream_modules} $1"
            have_$1="yes"
        ], [
            AC_MSG_RESULT([no])
            AS_IF([test "x${enable_$1}" = "xyes"], [
                # force on, but can't build
                AC_MSG_ERROR([m4_default([$4], [dependency checks failed]); pass --disable-$1 to disable this check])
            ], [
                # auto off
                AC_MSG_WARN([m4_default([$4], [dependency checks failed]); skipping])
            ])
        ])
    ], [
        # force off
        AC_MSG_RESULT([disabled by user])
    ])
    AM_CONDITIONAL([HAVE_STREAM_]$1_varname, [test "x${have_$1}" = "xyes"])
    m4_undefine([$1_varname])
])
