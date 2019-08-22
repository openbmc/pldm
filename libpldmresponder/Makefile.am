libpldmresponder_LTLIBRARIES = libpldmresponder.la
libpldmresponderdir = ${libdir}
libpldmresponder_la_SOURCES = \
	base.cpp \
	utils.cpp \
	bios_table.cpp \
	effecters.cpp \
	pdr.cpp \
	bios_parser.cpp \
	bios.cpp \
	platform.cpp
libpldmresponder_la_LIBADD = \
	../libpldm/libpldm.la \
	$(CODE_COVERAGE_LIBS)
libpldmresponder_la_LDFLAGS = \
	$(SDBUSPLUS_LIBS) \
	$(PHOSPHOR_LOGGING_LIBS) \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS) \
	-lstdc++fs \
	-version-info 1:0:0 -shared
libpldmresponder_la_CXXFLAGS = \
	$(CODE_COVERAGE_CXXFLAGS) \
	$(SDBUSPLUS_CFLAGS) \
	$(PHOSPHOR_LOGGING_CFLAGS) \
	$(PHOSPHOR_DBUS_INTERFACES_CFLAGS)

libpldmresponder_la_CPPFLAGS = \
	$(CODE_COVERAGE_CPPFLAGS) \
	-I$(top_srcdir)

if OEM_IBM
libpldmresponder_la_SOURCES += \
    $(top_builddir)/oem/ibm/libpldmresponder/file_io.cpp \
    $(top_builddir)/oem/ibm/libpldmresponder/file_table.cpp \
	$(top_builddir)/oem/ibm/libpldmresponder/file_io_by_type.cpp
libpldmresponder_la_CPPFLAGS += -I$(top_srcdir)/libpldm
endif

