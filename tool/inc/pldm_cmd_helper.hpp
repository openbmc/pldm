
#ifndef PLDM_CMD_HELPER_H
#define PLDM_CMD_HELPER_H

#include "logger.hpp"
#include "registration.hpp"

#include <err.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "libpldm/base.h"

/** @brief Print the buffer
 *
 *  @param[in]  buffer  - Buffer to print
 *
 *  @return - None
 */
void printBuffer(const std::vector<uint8_t>& buffer);

/** @brief MCTP socket intialization
 *
 *  None
 *
 *  @return - socket FD on success, -errno on failure
 */
int mctp_sock_init();

/** @brief MCTP socket read/recieve
 *
 *  @param[in]  socketFd - Socket file descriptor
 *  @param[in]  requestMsg - Requst message to compare against loopback
 *              message recieved from mctp
 *  @param[out] responseMsg - Response buffer recieved from mctp socket
 *
 *  @return -  PLDM_SUCCESS on success.
 *             -1 or -errno on failure.
 */
int mctp_sock_recv(int socketFd, std::vector<uint8_t> requestMsg,
                   std::vector<uint8_t>& responseMsg);

#endif
