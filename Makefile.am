SUBDIRS = libpldm libpldmresponder libmctp test

sbin_PROGRAMS = pldmd
pldmd_SOURCES = \
	pldmd.cpp \
	registration.cpp
pldmd_LDADD = \
	libpldmresponder/libpldmresponder.la \
	libmctp/libmctp.la
pldmd_LDFLAGS = \
	$(SYSTEMD_LIBS)
pldmd_CXXFLAGS = \
	$(SYSTEMD_CXXFLAGS)

if PLDMTOOL
SUBDIRS += tool
pldmd_SOURCES += pldmtool-receiver.cpp
endif
