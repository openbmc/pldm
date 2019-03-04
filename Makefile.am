SUBDIRS = libpldm libpldmresponder test

sbin_PROGRAMS = pldmd
pldmd_SOURCES = \
	pldmd.cpp \
	registration.cpp
pldmd_LDADD = \
	libpldmresponder/libpldmresponder.la

