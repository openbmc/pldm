AM_CPPFLAGS = -I$(top_srcdir) $(CODE_COVERAGE_CPPFLAGS)

TESTS = $(check_PROGRAMS)

check_PROGRAMS = \
	libpldm_base_test \
	libpldm_platform_test \
	libpldmresponder_base_test \
	libpldm_bios_test \
	libpldmresponder_bios_test \
	registration_test \
	libpldm_fileio_test \
	libpldmresponder_platform_test
  
test_cppflags = \
	-Igtest \
	$(GTEST_CPPFLAGS) \
	$(AM_CPPFLAGS)

test_cxxflags = \
	$(PTHREAD_CFLAGS) \
	$(CODE_COVERAGE_CXXFLAGS)

test_ldflags = \
	-lgtest_main \
	-lgtest \
	$(PTHREAD_LIBS) \
	$(OESDK_TESTCASE_FLAGS)

libpldm_base_test_CPPFLAGS = $(test_cppflags)
libpldm_base_test_CXXFLAGS = $(test_cxxflags)
libpldm_base_test_LDFLAGS = $(test_ldflags)
libpldm_base_test_LDADD = $(top_builddir)/libpldm/libpldm_la-base.o $(CODE_COVERAGE_LIBS)
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
libpldmresponder_bios_test_LDFLAGS = $(test_ldflags) $(SDBUSPLUS_LIBS)
libpldmresponder_bios_test_LDADD = \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-bios.o \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-utils.o \
	$(top_builddir)/libpldm/libpldm_la-base.o  \
	$(top_builddir)/libpldm/libpldm_la-bios.o \
	$(CODE_COVERAGE_LIBS) $(SDBUSPLUS_LIBS)
libpldmresponder_bios_test_SOURCES = \
	libpldmresponder_bios_test.cpp

libpldm_fileio_test_CPPFLAGS = $(test_cppflags)
libpldm_fileio_test_CXXFLAGS = $(test_cxxflags)
libpldm_fileio_test_LDFLAGS = $(test_ldflags)
libpldm_fileio_test_LDADD = \
	$(top_builddir)/libpldm/libpldm_la-base.o \
	$(top_builddir)/libpldm/libpldm_la-file_io.o
libpldm_fileio_test_SOURCES = libpldm_fileio_test.cpp

libpldmresponder_base_test_CPPFLAGS = $(test_cppflags)
libpldmresponder_base_test_CXXFLAGS = $(test_cxxflags)
libpldmresponder_base_test_LDFLAGS = $(test_ldflags)
libpldmresponder_base_test_LDADD = \
	$(top_builddir)/libpldm/libpldm_la-base.o \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-base.o $(CODE_COVERAGE_LIBS) \
  $(top_builddir)/registration.o

libpldmresponder_base_test_SOURCES = libpldmresponder_base_test.cpp

libpldmresponder_platform_test_CPPFLAGS = $(test_cppflags)
libpldmresponder_platform_test_CXXFLAGS = $(test_cxxflags)
libpldmresponder_platform_test_LDFLAGS = $(test_ldflags)
libpldmresponder_platform_test_LDADD = \
	$(top_builddir)/libpldm/libpldm_la-base.o \
	$(top_builddir)/libpldm/libpldm_la-platform.o \
	$(top_builddir)/registration.o \
	$(top_builddir)/libpldmresponder/libpldmresponder_la-platform.o
libpldmresponder_platform_test_SOURCES = libpldmresponder_platform_test.cpp

registration_test_CPPFLAGS = $(test_cppflags)
registration_test_CXXFLAGS = $(test_cxxflags)
registration_test_LDFLAGS = $(test_ldflags)
registration_test_LDADD = \
	$(top_builddir)/registration.o
registration_test_SOURCES = registration_test.cpp
