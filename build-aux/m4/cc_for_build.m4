# ===========================================================================
#   http://www.gnu.org/software/autoconf-archive/ax_prog_cc_for_build.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_PROG_CC_FOR_BUILD
#
# DESCRIPTION
#
#   This macro searches for a C compiler that generates native executables,
#   that is a C compiler that surely is not a cross-compiler. This can be
#   useful if you have to generate source code at compile-time like for
#   example GCC does.
#
#   The macro sets the CC_FOR_BUILD and CPP_FOR_BUILD macros to anything
#   needed to compile or link (CC_FOR_BUILD) and preprocess (CPP_FOR_BUILD).
#   The value of these variables can be overridden by the user by specifying
#   a compiler with an environment variable (like you do for standard CC).
#
#   It also sets BUILD_EXEEXT and BUILD_OBJEXT to the executable and object
#   file extensions for the build platform, and GCC_FOR_BUILD to `yes' if
#   the compiler we found is GCC. All these variables but GCC_FOR_BUILD are
#   substituted in the Makefile.
#
# LICENSE
#
#   Copyright (c) 2008 Paolo Bonzini <bonzini@gnu.org>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 8

AU_ALIAS([AC_PROG_CC_FOR_BUILD], [AX_PROG_CC_FOR_BUILD])

AC_DEFUN([AX_PROG_CC_FOR_BUILD], [
    AC_REQUIRE([AC_PROG_CC])
    AC_REQUIRE([AC_PROG_CPP])
    AC_REQUIRE([AC_EXEEXT])
    AC_REQUIRE([AC_CANONICAL_SYSTEM])
    AC_REQUIRE([LT_LIB_M])

    AS_IF([test "x${cross_compiling}" = "xyes" ], [
        #
        # Use the standard macros, but make them use other variable names
        #
        pushdef([ac_cv_prog_CPP], ac_cv_build_prog_CPP)
        pushdef([ac_cv_prog_gcc], ac_cv_build_prog_gcc)
        pushdef([ac_cv_prog_cc_works], ac_cv_build_prog_cc_works)
        pushdef([ac_cv_prog_cc_cross], ac_cv_build_prog_cc_cross)
        pushdef([ac_cv_prog_cc_g], ac_cv_build_prog_cc_g)
        pushdef([ac_cv_exeext], ac_cv_build_exeext)
        pushdef([ac_cv_objext], ac_cv_build_objext)
        pushdef([ac_cv_lib_m_cos], ac_cv_build_lib_m_cos)
        pushdef([ac_exeext], ac_build_exeext)
        pushdef([ac_objext], ac_build_objext)
        pushdef([CC], CC_FOR_BUILD)
        pushdef([CPP], CPP_FOR_BUILD)
        pushdef([CFLAGS], CFLAGS_FOR_BUILD)
        pushdef([CPPFLAGS], CPPFLAGS_FOR_BUILD)
        pushdef([LDFLAGS], LDFLAGS_FOR_BUILD)
        pushdef([LIBM], LIBM_FOR_BUILD)
        pushdef([host], build)
        pushdef([host_alias], build_alias)
        pushdef([host_cpu], build_cpu)
        pushdef([host_vendor], build_vendor)
        pushdef([host_os], build_os)
        pushdef([ac_cv_host], ac_cv_build)
        pushdef([ac_cv_host_alias], ac_cv_build_alias)
        pushdef([ac_cv_host_cpu], ac_cv_build_cpu)
        pushdef([ac_cv_host_vendor], ac_cv_build_vendor)
        pushdef([ac_cv_host_os], ac_cv_build_os)
        pushdef([ac_cpp], ac_build_cpp)
        pushdef([ac_compile], ac_build_compile)
        pushdef([ac_link], ac_build_link)

        save_cross_compiling=$cross_compiling
        save_ac_tool_prefix=$ac_tool_prefix
        cross_compiling=no
        ac_tool_prefix=

        AC_PROG_CC
        AC_PROG_CPP
        AC_EXEEXT
        LT_LIB_M

        ac_tool_prefix=$save_ac_tool_prefix
        cross_compiling=$save_cross_compiling

        #
        # Restore the old definitions
        #
        popdef([ac_link])
        popdef([ac_compile])
        popdef([ac_cpp])
        popdef([ac_cv_host_os])
        popdef([ac_cv_host_vendor])
        popdef([ac_cv_host_cpu])
        popdef([ac_cv_host_alias])
        popdef([ac_cv_host])
        popdef([host_os])
        popdef([host_vendor])
        popdef([host_cpu])
        popdef([host_alias])
        popdef([host])
        popdef([LIBM])
        popdef([LDFLAGS])
        popdef([CPPFLAGS])
        popdef([CFLAGS])
        popdef([CPP])
        popdef([CC])
        popdef([ac_objext])
        popdef([ac_exeext])
        popdef([ac_cv_lib_m_cos])
        popdef([ac_cv_objext])
        popdef([ac_cv_exeext])
        popdef([ac_cv_prog_cc_g])
        popdef([ac_cv_prog_cc_cross])
        popdef([ac_cv_prog_cc_works])
        popdef([ac_cv_prog_gcc])
        popdef([ac_cv_prog_CPP])

        BUILD_EXEEXT=$ac_build_exeext
        BUILD_OBJEXT=$ac_build_objext
    ], [
        CC_FOR_BUILD=${CC}
        CPP_FOR_BUILD=${CPP}
        CFLAGS_FOR_BUILD=${CFLAGS}
        CPPFLAGS_FOR_BUILD=${CPPFLAGS}
        LDFLAGS_FOR_BUILD=${LDFLAGS}
        LIBM_FOR_BUILD=${LIBM}

        BUILD_EXEEXT=${EXEEXT}
        BUILD_OBJEXT=${OBJEXT}
    ])

    #
    # Finally, set Makefile variables
    #
    AC_SUBST([BUILD_EXEEXT])
    AC_SUBST([BUILD_OBJEXT])
    AC_SUBST([CFLAGS_FOR_BUILD])
    AC_SUBST([CPPFLAGS_FOR_BUILD])
    AC_SUBST([LDFLAGS_FOR_BUILD])
    AC_SUBST([LIBM_FOR_BUILD])
])
