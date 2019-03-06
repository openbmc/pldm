libpldmresponder_LTLIBRARIES = libpldmresponder.la
libpldmresponderdir = ${libdir}
libpldmresponder_la_SOURCES = \
	base.cpp \
	utils.cpp \
	bios.cpp
libpldmresponder_la_LIBADD = \
	../libpldm/libpldm.la
libpldmresponder_la_LDFLAGS = \
	$(SDBUSPLUS_LIBS) $(PHOSPHOR_DBUS_INTERFACES_LIBS) \
	-version-info 1:0:0 -shared

libpldmresponder_la_CXXFLAGS = $(SDBUSPLUS_CFLAGS) \
	$(PHOSPHOR_DBUS_INTERFACES_CFLAGS)
