AM_CPPFLAGS		= -I$(srcdir)/../inc
AM_CPPFLAGS		+= -I../../

pldmtool_SOURCES 	= registration.cpp logger.cpp pldm_cmd_helper.cpp pldm_base_cmd.cpp pldmtool.cpp

pldmtool_LDADD = \
        ../../libpldm/libpldm.la \
        ../../libpldmresponder/libpldmresponder.la

bin_PROGRAMS		= pldmtool
