SUBDIRS = libpldm libpldmresponder libmctp test

sbin_PROGRAMS = pldmd
pldmd_LDADD = \
	libpldmresponder/libpldmresponder.la \
	libmctp/libmctp.la
pldmd_SOURCES = \
	pldmd.cpp \
	registration.cpp \
	mctp-lpc-setup.cpp
pldmd_LDFLAGS = \
	$(SYSTEMD_LIBS)
pldmd_CXXFLAGS = \
	$(SYSTEMD_CXXFLAGS)

if PLDMTOOL
pldmd_SOURCES += pldmtool-receiver.cpp
endif
