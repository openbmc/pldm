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

# Setup IBM OEM commands support for PLDM
AC_ARG_ENABLE([ibm-oem-support], AS_HELP_STRING([
	--enable-ibm-oem-support
	], [
	Enable PLDM OEM command support For IBM.
]))
AC_ARG_VAR(ENABLE_IBM_OEM_CODE, [Enable PLDM OEM command support For IBM.])
AS_IF([test "x$enable_ibm_oem_support" == "xyes"],[
	AC_SUBST(ENABLE_IBM_OEM_CODE, ["1"])
	AC_MSG_NOTICE([IBM OEM code support enabled - $ENABLE_IBM_OEM_CODE])
	],[
	AC_SUBST(ENABLE_IBM_OEM_CODE, ["0"])
	AC_MSG_NOTICE([IBM OEM code support disabled - $ENABLE_IBM_OEM_CODE])
])
AM_CONDITIONAL([ENABLE_IBM_OEM_CODE], [test "x$enable_ibmi_oem_support" = "xyes"])
AM_CONDITIONAL([ENABLE_IBM_OEM_CODE_SET], [test ! -z "$ENABLE_IBM_OEM_CODE"])

# Create configured output
AC_CONFIG_FILES([Makefile libpldm/Makefile libpldmresponder/Makefile test/Makefile])
AC_CONFIG_FILES([libpldm/libpldm.pc])
AC_OUTPUT
