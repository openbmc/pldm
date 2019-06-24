
#ifndef PLDM_CMD_HELPER_H
#define PLDM_CMD_HELPER_H

#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <err.h>
#include <unistd.h>

#include "registration.hpp"
#include "logger.hpp"
#include "libpldm/base.h"

/** @brief MCTP socket intialization
 *
 *  None
 *
 *  @return - socket FD on success, errno on error
 */
int mctp_mux_init();

#endif
