pldmtool_CXXFLAGS	= -I$(top_builddir)/tool

pldmtool_SOURCES 	= \
                          registration.cpp \
                          logger.cpp \
                          pldm_cmd_helper.cpp \
                          pldm_base_cmd.cpp \
                          pldmtool.cpp

pldmtool_LDADD = \
        ../../libpldm/libpldm.la \
        ../../libpldmresponder/libpldmresponder.la

sbin_PROGRAMS		= pldmtool
