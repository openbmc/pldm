#include "pldm_cmd_helper.hpp"

#include "common/transport.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <libpldm/transport.h>
#include <libpldm/transport/af-mctp.h>
#include <libpldm/transport/mctp-demux.h>
#include <poll.h>
#include <systemd/sd-bus.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <exception>

PHOSPHOR_LOG2_USING;

using namespace pldm::utils;

namespace pldmtool
{
namespace helper
{

void CommandInterface::exec()
{
    instanceId = instanceIdDb.next(mctp_eid);
    auto [rc, requestMsg] = createRequestMsg();
    if (rc != PLDM_SUCCESS)
    {
        instanceIdDb.free(mctp_eid, instanceId);
        error("Failed to encode request message for {TYPE}:{NAME} rc:{RC}",
              "TYPE", static_cast<std::string>(pldmType), "NAME",
              static_cast<std::string>(commandName), "RC", rc);
        return;
    }

    std::vector<uint8_t> responseMsg;
    rc = pldmSendRecv(requestMsg, responseMsg);

    if (rc != PLDM_SUCCESS)
    {
        instanceIdDb.free(mctp_eid, instanceId);
        error("pldmSendRecv: Failed to receive RC = {KEY0}", "KEY0", rc);
        return;
    }

    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg.data());
    parseResponseMsg(responsePtr, responseMsg.size() - sizeof(pldm_msg_hdr));
    instanceIdDb.free(mctp_eid, instanceId);
}

int CommandInterface::pldmSendRecv(std::vector<uint8_t>& requestMsg,
                                   std::vector<uint8_t>& responseMsg)
{
    // By default enable request/response msgs for pldmtool raw commands.
    if (CommandInterface::pldmType == "raw")
    {
        pldmVerbose = true;
    }

    if (pldmVerbose)
    {
        std::cout << "pldmtool: ";
        printBuffer(Tx, requestMsg);
    }

    auto tid = mctp_eid;
    PldmTransport pldmTransport{};
    uint8_t retry = 0;
    int rc = PLDM_ERROR;

    while (PLDM_REQUESTER_SUCCESS != rc && retry <= numRetries)
    {
        void* responseMessage = nullptr;
        size_t responseMessageSize{};

        rc = pldmTransport.sendRecvMsg(tid, requestMsg.data(),
                                       requestMsg.size(), responseMessage,
                                       responseMessageSize);
        if (rc)
        {
            std::cerr << "[" << unsigned(retry) << "] pldm_send_recv error rc "
                      << rc << std::endl;
            retry++;
            continue;
        }

        responseMsg.resize(responseMessageSize);
        memcpy(responseMsg.data(), responseMessage, responseMsg.size());

        free(responseMessage);

        if (pldmVerbose)
        {
            std::cout << "pldmtool: ";
            printBuffer(Rx, responseMsg);
        }
    }

    if (rc)
    {
        error("failed to pldm send recv error rc:{RC}", "RC", rc);
    }

    return rc;
}
} // namespace helper
} // namespace pldmtool
