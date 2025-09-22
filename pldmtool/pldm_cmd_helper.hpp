#pragma once

#include "common/instance_id.hpp"
#include "common/utils.hpp"
#include "pldm_rde_cmd.hpp"

#include <err.h>
#include <libpldm/base.h>
#include <libpldm/bios.h>
#include <libpldm/fru.h>
#include <libpldm/platform.h>
#include <linux/mctp.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include <cstring>
#include <iomanip>
#include <iostream>
#include <utility>

namespace pldmtool
{

namespace helper
{

constexpr uint8_t PLDM_ENTITY_ID = 8;
constexpr static uint8_t MCTP_MSG_TYPE_PLDM = 1;
using ordered_json = nlohmann::ordered_json;
constexpr static auto mctpEndpointIntfName{"xyz.openbmc_project.MCTP.Endpoint"};
constexpr static auto unixSocketIntfName{
    "xyz.openbmc_project.Common.UnixSocket"};
constexpr static auto objectEnableIntfName{"xyz.openbmc_project.Object.Enable"};

/** @brief print the input message if pldmverbose is enabled
 *
 *  @param[in]  pldmVerbose - verbosity flag - true/false
 *  @param[in]  msg         - message to print
 *  @param[in]  data        - data to print
 *
 *  @return - None
 */

template <class T>
void Logger(bool pldmverbose, const char* msg, const T& data)
{
    if (pldmverbose)
    {
        std::stringstream s;
        s << data;
        std::cout << msg << s.str() << std::endl;
    }
}

/** @brief Display in JSON format.
 *
 *  @param[in]  data - data to print in json
 *
 *  @return - None
 */
static inline void DisplayInJson(const ordered_json& data)
{
    std::cout << data.dump(4) << std::endl;
}

/**
 * @brief Maps numeric PLDM completion codes to human-readable strings
 *
 * @param[in] completionCode - Numeric PLDM completion code returned in response
 * @param[in] data - The JSON data to which the completion code is added.
 * @param[in] pldmType - PLDM type
 */
void fillCompletionCode(uint8_t completionCode, ordered_json& data,
                        uint8_t pldmType);

/** @brief MCTP socket read/receive
 *
 *  @param[in]  mctpNetworkId - mctp network id
 *  @param[in]  eid - mctp endpoint id
 *  @mctpPreAllocTag - bool to indicate request for preallocated tag
 *  @param[in]  requestMsg - Request message to compare against loopback
 *              message received from mctp socket
 *  @param[out] responseMsg - Response buffer received from mctp socket
 *
 *  @return -   0 on success.
 *             -1 or -errno on failure.
 */
int mctpSockSendRecv(const uint8_t mctpNetworkId, const uint8_t eid,
                     const bool mctpPreAllocTag,
                     const std::vector<uint8_t>& requestMsg,
                     void** responseMessage, size_t* responseMessageSize);

class CommandInterface
{
  public:
    explicit CommandInterface(const char* type, const char* name,
                              CLI::App* app) :
        pldmType(type), commandName(name), mctp_eid(PLDM_ENTITY_ID),
        pldmVerbose(false), instanceId(0)
    {
        app->add_option("-m,--mctp_eid", mctp_eid, "MCTP endpoint ID");
        app->add_flag("-v, --verbose", pldmVerbose);
        app->add_option("-n, --retry-count", numRetries,
                        "Number of retry when PLDM request message is failed");
        app->callback([&]() { exec(); });
    }

    virtual ~CommandInterface() = default;

    virtual std::pair<int, std::vector<uint8_t>> createRequestMsg() = 0;

    virtual void parseResponseMsg(struct pldm_msg* responsePtr,
                                  size_t payloadLength) = 0;

    virtual void exec();

    int pldmSendRecv(std::vector<uint8_t>& requestMsg,
                     std::vector<uint8_t>& responseMsg);

    /**
     * @brief get MCTP endpoint ID
     *
     * @return uint8_t - MCTP endpoint ID
     */
    inline uint8_t getMCTPEID()
    {
        return mctp_eid;
    }

    /**
     * @brief get PLDM type
     *
     * @return pldm type
     */
    inline std::string getPLDMType()
    {
        return pldmType;
    }

    /**
     * @brief get command name
     *
     * @return  the command name
     */
    inline std::string getCommandName()
    {
        return commandName;
    }

  private:
    const std::string pldmType;
    const std::string commandName;
    uint8_t mctp_eid;
    bool pldmVerbose;
    pldm::dbus::ObjectValueTree getMctpManagedObjects(
        const std::string& service) const noexcept;
    std::set<pldm::dbus::Service> getMctpServices() const;
    std::tuple<bool, int, int, std::vector<uint8_t>> getMctpSockInfo(
        uint8_t remoteEID);

  protected:
    uint8_t instanceId;
    pldm::InstanceIdDb instanceIdDb;
    uint8_t numRetries = 0;
    bool mctpPreAllocTag = false;
    uint8_t mctpNetworkId = 1;
};

} // namespace helper
} // namespace pldmtool
