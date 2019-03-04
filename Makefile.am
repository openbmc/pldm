SUBDIRS = libpldm libpldmresponder libmctp test

sbin_PROGRAMS = pldmd
pldmd_SOURCES = \
	pldmd.cpp \
	registration.cpp
pldmd_LDADD = \
	libpldmresponder/libpldmresponder.la \
	libmctp/libmctp.la

