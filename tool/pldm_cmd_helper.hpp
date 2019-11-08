#pragma once

#ifndef PLDM_CMD_HELPER_H
#define PLDM_CMD_HELPER_H

#include "libpldmresponder/utils.hpp"

#include <err.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <iomanip>
#include <iostream>
#include <utility>

#include "libpldm/base.h"
#include "libpldm/platform.h"

using namespace pldm::responder::utils;

/** @brief Print the buffer
 *
 *  @param[in]  buffer  - Buffer to print
 *
 *  @return - None
 */
void printBuffer(const std::vector<uint8_t>& buffer);

/** @brief MCTP socket read/recieve
 *
 *  @param[in]  requestMsg - Request message to compare against loopback
 *              message recieved from mctp socket
 *  @param[out] responseMsg - Response buffer recieved from mctp socket
 *
 *  @return -   0 on success.
 *             -1 or -errno on failure.
 */
int mctpSockSendRecv(const std::vector<uint8_t>& requestMsg,
                     std::vector<uint8_t>& responseMsg);

class CommandInterface
{
  public:
    CommandInterface(std::string name) : commandName(name)
    {
    }
    virtual ~CommandInterface() = default;

    virtual std::pair<int, std::vector<uint8_t>>
        createRequestMsg(std::vector<std::string>& args) = 0;

    virtual void parseResponseMsg(std::vector<uint8_t>& responseMsg) = 0;

    void exec(std::vector<std::string>& args);

    const std::string commandName;
};

#endif
