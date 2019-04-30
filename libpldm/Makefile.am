libpldm_includedir = $(includedir)/libpldm
libpldm_include_HEADERS = \
	base.h \
	pldm_types.h \
	platform.h \
	bios.h

libpldm_LTLIBRARIES = libpldm.la
libpldmdir = ${libdir}
libpldm_la_SOURCES = \
	base.c \
	platform.c \
	bios.c
libpldm_la_LDFLAGS = -version-info 1:0:0 -shared
libpldm_la_CFLAGS = $(CODE_COVERAGE_CFLAGS)
libpldm_la_LIBADD = $(CODE_COVERAGE_LIBS)

pkgconfiglibdir = ${libdir}/pkgconfig
pkgconfiglib_DATA = libpldm.pc
