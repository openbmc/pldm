libpldmresponder_LTLIBRARIES = libpldmresponder.la
libpldmresponderdir = ${libdir}
libpldmresponder_la_SOURCES = \
	base.cpp \
	platform.cpp
libpldmresponder_la_LIBADD = \
	../libpldm/libpldm.la
libpldmresponder_la_LDFLAGS = \
	-version-info 1:0:0 -shared
