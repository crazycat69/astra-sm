#
# Custom macros for Astra SM
#

# AX_HELP_SECTION
#----------------
AC_DEFUN([AX_HELP_SECTION], [m4_divert_once([HELP_ENABLE], [
$1])])

# AX_SAVE_FLAGS
# -------------
AC_DEFUN([AX_SAVE_FLAGS], [
    SAVED_CFLAGS="$CFLAGS"
    SAVED_LIBS="$LIBS"
])

# AX_RESTORE_FLAGS
#-----------------
AC_DEFUN([AX_RESTORE_FLAGS], [
    CFLAGS="$SAVED_CFLAGS"
    LIBS="$SAVED_LIBS"
])

# AX_EXTLIB_VARS(LIBNAME)
#------------------------
AC_DEFUN([AX_EXTLIB_VARS], [
    # makefile variables
    m4_define([$1_ucase],[m4_translit([$1], [a-z], [A-Z])])

    AS_IF([test "x${with_$1_includes}" != "xno"],
        $1_ucase[_CFLAGS="-I${with_$1_includes}"])
    AS_IF([test "x${with_$1_libs}" != "xno"],
        $1_ucase[_LIBS="-L${with_$1_libs}"])

    AC_ARG_VAR($1_ucase[_CFLAGS], [C compiler flags for $1])
    AC_ARG_VAR($1_ucase[_LIBS], [linker flags for $1])

    m4_undefine([$1_ucase])
])

# AX_EXTLIB_OPTIONAL(LIBNAME, DESCRIPTION)
#-----------------------------------------
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

# AX_EXTLIB_PARAM(LIBNAME, DESCRIPTION)
#--------------------------------------
AC_DEFUN([AX_EXTLIB_PARAM], [
    AX_EXTLIB_OPTIONAL($1, $2)
    AX_EXTLIB_VARS($1)
])

# AX_DVBAPI([ACTION-IF-SUCCESS], [ACTION-IF-FAILURE])
#----------------------------------------------------
# Check presence of the Linux DVB API
AC_DEFUN([AX_DVBAPI], [
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
        AC_DEFINE([HAVE_DVBAPI], [1],
            [Define to 1 if you have a usable DVB API])

        # DVB-T2 (DVB API >= 5.3)
        AC_MSG_CHECKING([for DVB-T2 support])
        AC_COMPILE_IFELSE([
            AC_LANG_PROGRAM([[
                #include <linux/dvb/frontend.h>
            ]], [[
                fe_delivery_system_t sys = SYS_DVBT2;
                fe_transmit_mode_t tm;
                tm = TRANSMISSION_MODE_1K;
                tm = TRANSMISSION_MODE_16K;
                tm = TRANSMISSION_MODE_32K;
            ]])
        ], [
            AC_MSG_RESULT([yes])
            AC_DEFINE([HAVE_DVBAPI_T2], [1],
                [Define to 1 if your system supports DVB-T2])
        ], [
            AC_MSG_RESULT([no])
            AC_MSG_WARN([DVB API 5.3 or higher required for DVB-T2 support])
        ])

        # DVB-C annex A/C defines (DVB API >= 5.6)
        AC_MSG_CHECKING([DVB-C annex A/C definitions])
        AC_COMPILE_IFELSE([
            AC_LANG_PROGRAM([[
                #include <linux/dvb/frontend.h>
            ]], [[
                fe_delivery_system_t sys;
                sys = SYS_DVBC_ANNEX_A;
                sys = SYS_DVBC_ANNEX_C;
            ]])
        ], [
            AC_MSG_RESULT([separate A and C])
            AC_DEFINE([HAVE_DVBAPI_C_ANNEX_AC], [1],
                [Define to 1 if you have separate constants for DVB-C annex A and C])
        ], [
            AC_MSG_RESULT([single constant])
        ])

        # Network header (not present on FreeBSD)
        AC_CHECK_HEADERS([linux/dvb/net.h])

        m4_default([$1], [])
    ], [
        AC_MSG_RESULT([no])
        m4_default([$2], [])
    ])
])

# AX_CHECK_CFLAG(FLAG, [FLAGVAR], [ACTION-IF-SUCCESS], [ACTION-IF-FAILURE])
#--------------------------------------------------------------------------
# Check if $CC supports FLAG; append it to FLAGVAR on success.
# If specified, perform ACTION-IF-SUCCESS or ACTION-IF-FAILURE.
AC_DEFUN([AX_CHECK_CFLAG], [
    AC_MSG_CHECKING([whether $CC accepts $1])

    AX_SAVE_FLAGS
    CFLAGS="-Werror $1"
    AC_COMPILE_IFELSE([
        AC_LANG_SOURCE([[
            int main(void);
            int main(void)
            {
                return 0;
            }
        ]])
    ], [ ccf_have_flag="yes" ], [ ccf_have_flag="no" ])
    AX_RESTORE_FLAGS

    AS_IF([test "x${ccf_have_flag}" = "xyes"], [
        AC_MSG_RESULT([yes])
        m4_define([ccf_flag_var], m4_default([$2], [CFLAGS]))
        ccf_flag_var="$ccf_flag_var $1"
        m4_undefine([ccf_flag_var])
        m4_default([$3], [])
    ], [
        AC_MSG_RESULT([no])
        m4_default([$4], [])
    ])
])

# AX_CHECK_CFLAGS(FLAGLIST, [FLAGVAR], [ACTION-IF-SUCCESS], [ACTION-IF-FAILURE])
#-------------------------------------------------------------------------------
# For each flag in FLAGLIST perform AX_CHECK_CFLAG
AC_DEFUN([AX_CHECK_CFLAGS], [
    for ac_flag in $1; do
        AX_CHECK_CFLAG([$ac_flag], [$2], [$3], [$4])
    done
])
