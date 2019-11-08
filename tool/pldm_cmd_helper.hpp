#pragma once

#ifndef PLDM_CMD_HELPER_H
#define PLDM_CMD_HELPER_H

#include "libpldmresponder/utils.hpp"

#include <err.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <CLI/CLI.hpp>
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
    CommandInterface(const char* type, const char* name, CLI::App* app) :
        pldmType(type), commandName(name)
    {
        app->callback([&]() { exec(); });
    }
    virtual ~CommandInterface() = default;

    virtual std::pair<int, std::vector<uint8_t>> createRequestMsg() = 0;

    virtual void parseResponseMsg(struct pldm_msg* responsePtr,
                                  size_t payloadLength) = 0;

    void exec();

  private:
    const std::string pldmType;
    const std::string commandName;
};

#endif
