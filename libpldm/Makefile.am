nobase_include_HEADERS = \
	base.h \
        file_io.h \
	platform.h

libpldm_LTLIBRARIES = libpldm.la
libpldmdir = ${libdir}
libpldm_la_SOURCES = \
	base.c \
        file_io.c \
	platform.c

libpldm_la_LDFLAGS = \
        -version-info 1:0:0 -shared

pkgconfiglibdir = ${libdir}/pkgconfig
pkgconfiglib_DATA = libpldm.pc
