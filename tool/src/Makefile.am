AM_CPPFLAGS		= -I$(srcdir)/../inc
AM_CPPFLAGS		+= -I../../

pldmtool_SOURCES 	= pldmtool.cpp pt_log.cpp pt_base_cmd.cpp


pldmtool_LDADD = \
	../../libmctp/libmctp.la \
	../../libpldm/libpldm.la \
 	../../libpldmresponder/libpldmresponder.la

bin_PROGRAMS		= pldmtool
