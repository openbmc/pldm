# Initialization
AC_PREREQ([2.69])
AC_INIT([pldm], [1.0],
        [https://github.com/openbmc/pldm/issues])
AC_LANG([C++])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign dist-xz])
AM_SILENT_RULES([yes])

# Make sure the default CFLAGS of `-O2 -g` don't override CODE_COVERAGE_CFLAGS
# It is important that this comes before AC_PROG_C{C,XX}, as we are attempting
# to stop them from populating default CFLAGS and CXXFLAGS.
#AS_IF([test "x$enable_tests" = "xno"], [enable_code_coverage=no])
AS_IF([test "x$enable_code_coverage" != "xno"], [
    AS_IF([test "x${CXXFLAGS+set}" != "xset"], [
        AC_SUBST(CXXFLAGS, [""])
    ])
    AS_IF([test "x${CFLAGS+set}" != "xset"], [
        AC_SUBST(CFLAGS, [""])
    ])
])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AM_PROG_AR
AC_PROG_INSTALL

# Checks for typedefs, structures, and compiler characteristics.
AX_CXX_COMPILE_STDCXX([17], [noext], [mandatory])
AX_APPEND_COMPILE_FLAGS([-Wall -Werror], [CXXFLAGS])

# Check for packages
AX_PKG_CHECK_MODULES([SYSTEMD], [], [libsystemd >= 221])

# For linking
LT_INIT

# Code coverage
AX_CODE_COVERAGE
AM_EXTRA_RECURSIVE_TARGETS([check-code-coverage])
m4_ifdef([_AX_CODE_COVERAGE_RULES],
    [AM_CONDITIONAL(AUTOCONF_CODE_COVERAGE_2019_01_06, [true])],
    [AM_CONDITIONAL(AUTOCONF_CODE_COVERAGE_2019_01_06, [false])])
AX_ADD_AM_MACRO_STATIC([])

AX_PKG_CHECK_MODULES([SDBUSPLUS], [sdbusplus])

# Check/set gtest specific functions.
AX_PTHREAD([GTEST_CPPFLAGS="-DGTEST_HAS_PTHREAD=1"],[GTEST_CPPFLAGS="-DGTEST_HAS_PTHREAD=0"])
AC_SUBST(GTEST_CPPFLAGS)
AC_ARG_ENABLE([oe-sdk],
    AS_HELP_STRING([--enable-oe-sdk], [Link testcases absolutely against OE SDK so they can be ran within it.])
)
AC_ARG_VAR(OECORE_TARGET_SYSROOT,
    [Path to the OE SDK SYSROOT])
AS_IF([test "x$enable_oe_sdk" == "xyes"],
    AS_IF([test "x$OECORE_TARGET_SYSROOT" == "x"],
          AC_MSG_ERROR([OECORE_TARGET_SYSROOT must be set with --enable-oe-sdk])
    )
    AC_MSG_NOTICE([Enabling OE-SDK at $OECORE_TARGET_SYSROOT])
    [
        testcase_flags="-Wl,-rpath,\${OECORE_TARGET_SYSROOT}/lib"
        testcase_flags="${testcase_flags} -Wl,-rpath,\${OECORE_TARGET_SYSROOT}/usr/lib"
        testcase_flags="${testcase_flags} -Wl,-dynamic-linker,`find \${OECORE_TARGET_SYSROOT}/lib/ld-*.so | sort -r -n | head -n1`"
    ]
    AC_SUBST([OESDK_TESTCASE_FLAGS], [$testcase_flags])
)

AC_ARG_ENABLE([pldmtool],
[  --enable-debug    Turn on pldmtool support],
[case "${enableval}" in
  yes) pldmtool=true ;;
  no)  pldmtool=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-pldmtool]) ;;
esac],[pldmtool=false])
AM_CONDITIONAL([PLDMTOOL], [test x$pldmtool = xtrue])
AS_IF([test "x$pldmtool" == "xtrue"], [
    AX_APPEND_COMPILE_FLAGS([-DPLDMTOOL], [CXXFLAGS])
])

# Create configured output
AC_CONFIG_FILES([Makefile libpldm/Makefile libpldmresponder/Makefile \
    libmctp/Makefile tool/Makefile test/Makefile])
AC_CONFIG_FILES([libpldm/libpldm.pc])
AC_OUTPUT
