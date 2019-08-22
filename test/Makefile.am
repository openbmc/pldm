AM_CPPFLAGS = -I$(top_srcdir) $(CODE_COVERAGE_CPPFLAGS)

TESTS = $(check_PROGRAMS)

check_PROGRAMS = \
	libpldm_base_test \
	libpldm_platform_test \
	libpldmresponder_base_test \
	libpldm_bios_test \
	libpldmresponder_bios_test \
	libpldmresponder_pdr_state_effecter_test \
	libpldmresponder_bios_table_test \
	libpldmresponder_platform_test

if OEM_IBM
check_PROGRAMS += \
	libpldmoem_fileio_test \
	libpldmoemresponder_fileio_test
endif

test_cppflags = \
	-Igtest \
	$(GTEST_CPPFLAGS) \
	$(AM_CPPFLAGS)

test_cxxflags = \
	$(PTHREAD_CFLAGS) \
	$(PHOSPHOR_LOGGING_CFLAGS) \
	$(SDBUSPLUS_CFLAGS) \
	$(CODE_COVERAGE_CXXFLAGS) \
	$(PHOSPHOR_DBUS_INTERFACES_CFLAGS)

test_ldflags = \
	-lgtest_main \
	-lgtest \
	-lgmock \
	-lstdc++fs \
	$(PTHREAD_LIBS) \
	$(SDBUSPLUS_LIBS) \
	$(PHOSPHOR_LOGGING_LIBS) \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS) \
	$(OESDK_TESTCASE_FLAGS)

if OEM_IBM
test_ldflags += \
	-lgmock \
	-lstdc++fs
endif

libpldm_base_test_CPPFLAGS = $(test_cppflags)
libpldm_base_test_CXXFLAGS = $(test_cxxflags)
libpldm_base_test_LDFLAGS = $(test_ldflags)
libpldm_base_test_LDADD = \
	$(top_builddir)/libpldm/libpldm_la-base.o \
	$(CODE_COVERAGE_LIBS)
libpldm_base_test_SOURCES = libpldm_base_test.cpp

libpldm_platform_test_CPPFLAGS = $(test_cppflags)
libpldm_platform_test_CXXFLAGS = $(test_cxxflags)
libpldm_platform_test_LDFLAGS = $(test_ldflags)
libpldm_platform_test_LDADD = \
	$(top_builddir)/libpldm/libpldm_la-platform.o \
	$(top_builddir)/libpldm/libpldm_la-base.o \
	$(CODE_COVERAGE_LIBS)
libpldm_platform_test_SOURCES = libpldm_platform_test.cpp

libpldm_bios_test_CPPFLAGS = $(test_cppflags)
libpldm_bios_test_CXXFLAGS = $(test_cxxflags)
libpldm_bios_test_LDFLAGS = $(test_ldflags)
libpldm_bios_test_LDADD = \
	$(top_builddir)/libpldm/libpldm_la-base.o  \
	$(top_builddir)/libpldm/libpldm_la-bios.o \
	$(CODE_COVERAGE_LIBS)
libpldm_bios_test_SOURCES = libpldm_bios_test.cpp

libpldmresponder_bios_test_CPPFLAGS = $(test_cppflags)
libpldmresponder_bios_test_CXXFLAGS = $(test_cxxflags)
libpldmresponder_bios_test_LDFLAGS = \
	$(test_ldflags) \
	$(SDBUSPLUS_LIBS) \
	-lstdc++fs
libpldmresponder_bios_test_LDADD = \
	$(top_builddir)/pldmd-registration.o \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-bios.o \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-utils.o \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-bios_table.o \
	$(top_builddir)/libpldm/libpldm_la-base.o  \
	$(top_builddir)/libpldm/libpldm_la-bios.o \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-bios_parser.o \
	$(CODE_COVERAGE_LIBS) \
	$(PHOSPHOR_LOGGING_LIBS) \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS) \
	$(SDBUSPLUS_LIBS) \
	-lstdc++fs
libpldmresponder_bios_test_SOURCES = \
	libpldmresponder_bios_test.cpp

libpldmresponder_base_test_CPPFLAGS = $(test_cppflags)
libpldmresponder_base_test_CXXFLAGS = $(test_cxxflags)
libpldmresponder_base_test_LDFLAGS = $(test_ldflags)
libpldmresponder_base_test_LDADD = \
	$(top_builddir)/pldmd-registration.o \
	$(top_builddir)/libpldm/libpldm_la-base.o \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-base.o \
	$(CODE_COVERAGE_LIBS)
libpldmresponder_base_test_SOURCES = libpldmresponder_base_test.cpp

if OEM_IBM
libpldmoem_fileio_test_CPPFLAGS = $(test_cppflags) \
	-I$(top_builddir)/oem/ibm/ \
	-I$(top_builddir)/libpldm/
libpldmoem_fileio_test_CXXFLAGS = $(test_cxxflags)
libpldmoem_fileio_test_LDFLAGS = $(test_ldflags)
libpldmoem_fileio_test_LDADD = \
	$(top_builddir)/libpldm/libpldm_la-base.o \
	$(top_builddir)/oem/ibm/libpldm/libpldm_la-file_io.o
libpldmoem_fileio_test_SOURCES = $(top_builddir)/oem/ibm/test/libpldm_fileio_test.cpp

libpldmoemresponder_fileio_test_CPPFLAGS = $(test_cppflags) \
	-I$(top_builddir)/oem/ibm/ \
	-I$(top_builddir)/libpldm/
libpldmoemresponder_fileio_test_CXXFLAGS = $(test_cxxflags)
libpldmoemresponder_fileio_test_LDFLAGS = $(test_ldflags) \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS)
libpldmoemresponder_fileio_test_LDADD = \
	$(top_builddir)/pldmd-registration.o \
	$(top_builddir)/libpldm/libpldm_la-base.o \
	$(top_builddir)/oem/ibm/libpldm/libpldm_la-file_io.o \
	$(top_builddir)/oem/ibm/libpldmresponder/libpldmresponder_la-file_io.o\
	$(top_builddir)/oem/ibm/libpldmresponder/libpldmresponder_la-file_table.o \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-utils.o \
	$(top_builddir)/oem/ibm/libpldmresponder/libpldmresponder_la-file_io_by_type.o \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS)
libpldmoemresponder_fileio_test_SOURCES = $(top_builddir)/oem/ibm/test/libpldmresponder_fileio_test.cpp
endif

libpldmresponder_pdr_state_effecter_test_CPPFLAGS = $(test_cppflags)
libpldmresponder_pdr_state_effecter_test_CXXFLAGS = $(test_cxxflags)
libpldmresponder_pdr_state_effecter_test_LDFLAGS = $(test_ldflags)
libpldmresponder_pdr_state_effecter_test_LDADD = \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-pdr.o \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-effecters.o \
	$(CODE_COVERAGE_LIBS) \
	$(SDBUSPLUS_LIBS) \
	$(PHOSPHOR_LOGGING_LIBS) \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS) \
	-lstdc++fs
libpldmresponder_pdr_state_effecter_test_SOURCES = libpldmresponder_pdr_state_effecter_test.cpp

libpldmresponder_bios_table_test_CPPFLAGS = $(test_cppflags)
libpldmresponder_bios_table_test_CXXFLAGS = $(test_cxxflags)
libpldmresponder_bios_table_test_LDFLAGS = \
	$(test_ldflags) \
	$(SDBUSPLUS_LIBS)
libpldmresponder_bios_table_test_LDADD = \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-bios.o \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-bios_table.o \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-utils.o \
	$(top_builddir)/libpldm/libpldm_la-base.o  \
	$(top_builddir)/libpldm/libpldm_la-bios.o \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-bios_parser.o \
	$(top_builddir)/pldmd-registration.o \
	$(CODE_COVERAGE_LIBS) \
	$(SDBUSPLUS_LIBS) \
	$(PHOSPHOR_LOGGING_LIBS) \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS) \
	-lstdc++fs
libpldmresponder_bios_table_test_SOURCES = \
        libpldmresponder_bios_table_test.cpp

libpldmresponder_platform_test_CPPFLAGS = $(test_cppflags)
libpldmresponder_platform_test_CXXFLAGS = $(test_cxxflags)
libpldmresponder_platform_test_LDFLAGS = $(test_ldflags) $(SDBUSPLUS_LIBS)
libpldmresponder_platform_test_LDADD = \
	$(top_builddir)/pldmd-registration.o \
	$(top_builddir)/libpldm/libpldm_la-base.o \
	$(top_builddir)/libpldm/libpldm_la-platform.o \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-pdr.o \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-effecters.o \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-platform.o \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-utils.o
	$(PHOSPHOR_LOGGING_LIBS) \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS) \
	$(SDBUSPLUS_LIBS) \
	$(CODE_COVERAGE_LIBS) \
	-lstdc++fs
libpldmresponder_platform_test_SOURCES = libpldmresponder_platform_test.cpp
