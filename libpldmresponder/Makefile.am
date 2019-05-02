libpldmresponder_LTLIBRARIES = libpldmresponder.la
libpldmresponderdir = ${libdir}
libpldmresponder_la_SOURCES = \
	base.cpp \
	utils.cpp \
	bios.cpp \
	effecters.cpp \
	pdr.cpp
libpldmresponder_la_LIBADD = \
	../libpldm/libpldm.la \
	$(CODE_COVERAGE_LIBS)
libpldmresponder_la_LDFLAGS = \
	$(SDBUSPLUS_LIBS) \
	-version-info 1:0:0 -shared
libpldmresponder_la_CXXFLAGS = \
	$(CODE_COVERAGE_CXXFLAGS) \
	$(SDBUSPLUS_CFLAGS)

libpldmresponder_la_CPPFLAGS = \
	$(CODE_COVERAGE_CPPFLAGS) \
	-I$(top_srcdir)


