#pragma once

#include "utils.hpp"

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
#include "libpldm/bios.h"
#include "libpldm/fru.h"
#include "libpldm/platform.h"

namespace pldmtool
{

namespace helper
{

using namespace pldm::utils;
constexpr uint8_t PLDM_ENTITY_ID = 8;
constexpr uint8_t MCTP_MSG_TYPE_PLDM = 1;
constexpr uint8_t PLDM_LOCAL_INSTANCE_ID = 0;
constexpr int PLDM_VERBOSE = 0;

/** @brief Print the buffer
 *
 *  @param[in]  buffer  - Buffer to print
 *  @param[in]  pldmVerbose - enable verbose 0/1
 *
 *  @return - None
 */
void printBuffer(const std::vector<uint8_t>& buffer, int pldmVerbose);

/** @brief print the input message if pldmverbose is enabled
 *
 *  @param[in]  pldmVerbose - enable verbose 0/1
 *  @param[in]  msg         - message to print
 *  @param[in]  data        - data to print
 *
 *  @return - None
 */
template <class T>
void Logger(int pldmverbose, const char* msg, const T& data)
{
    if (pldmverbose)
    {
        std::stringstream s;
        s << data;
        std::cout << msg << s.str() << std::endl;
    }
}
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
                     std::vector<uint8_t>& responseMsg, int pldmVerbose);

class CommandInterface
{

  public:
    explicit CommandInterface(const char* type, const char* name,
                              CLI::App* app) :
        pldmType(type),
        commandName(name), mctp_eid(PLDM_ENTITY_ID), pldmVerbose(PLDM_VERBOSE)
    {
        app->add_option("-m,--mctp_eid", mctp_eid, "MCTP endpoint ID");
        app->add_option("-v, --verbose", pldmVerbose, "Enable verbose 0/1");
        app->callback([&]() { exec(); });
    }
    virtual ~CommandInterface() = default;

    virtual std::pair<int, std::vector<uint8_t>> createRequestMsg() = 0;

    virtual void parseResponseMsg(struct pldm_msg* responsePtr,
                                  size_t payloadLength) = 0;

    virtual void exec();

    int pldmSendRecv(std::vector<uint8_t>& requestMsg,
                     std::vector<uint8_t>& responseMsg);

  private:
    const std::string pldmType;
    const std::string commandName;
    uint8_t mctp_eid;
    int pldmVerbose;
};

} // namespace helper
} // namespace pldmtool
