# Initialization
AC_PREREQ([2.69])
AC_INIT([pldmtool], [1.0],
        [https://github.com/openbmc/pldm/issues])

AC_LANG([C++])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign dist-xz])
AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AM_PROG_AR
AC_PROG_INSTALL

# Check for packages
AX_PKG_CHECK_MODULES([SYSTEMD], [], [libsystemd >= 221])

# Checks for typedefs, structures, and compiler characteristics.
AX_CXX_COMPILE_STDCXX([17], [noext], [mandatory])
AX_APPEND_COMPILE_FLAGS([-Wall -Werror], [CXXFLAGS])

# For linking
LT_INIT

AC_ARG_ENABLE([pldmtool],
[  --enable-pldmtool    Turn on pldmtool support],
[case "${enableval}" in
yes) pldmtool=true ;;
no)  pldmtool=false ;;
*) AC_MSG_ERROR([bad value ${enableval} for --enable-pldmtool]) ;;
esac],[pldmtool=false])
AM_CONDITIONAL([PLDMTOOL], [test x$pldmtool = xtrue])
AS_IF([test "x$pldmtool" == "xtrue"], [
AX_APPEND_COMPILE_FLAGS([-DPLDMTOOL], [CXXFLAGS])
])

#checks for programs.
#checks for libraries
#checks for header files
#checks for types
#checks for structures
#checks for compiler characteristics
#checks for library functions
#checks for system services
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
