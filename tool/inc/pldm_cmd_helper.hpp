
#ifndef PLDM_CMD_HELPER_H
#define PLDM_CMD_HELPER_H

#include <err.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>

#include "libpldm/base.h"
#include "logger.hpp"
#include "registration.hpp"

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
int  mctp_sock_recv(int socketFd, std::vector<uint8_t> requestMsg,
                    std::vector<uint8_t> &responseMsg);

#endif
