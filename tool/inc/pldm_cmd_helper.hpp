
#ifndef PLDM_CMD_HELPER_H
#define PLDM_CMD_HELPER_H

#include "logger.hpp"
#include "registration.hpp"

#include <err.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>

#include "libpldm/base.h"

/** @brief MCTP socket intialization
 *
 *  None
 *
 *  @return - socket FD on success, errno on error
 */
int mctp_mux_init();

#endif
