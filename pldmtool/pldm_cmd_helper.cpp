#include "pldm_cmd_helper.hpp"

#include "common/transport.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <libpldm/firmware_update.h>
#include <libpldm/transport.h>
#include <libpldm/transport/af-mctp.h>
#include <libpldm/transport/mctp-demux.h>
#include <poll.h>
#include <systemd/sd-bus.h>

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <exception>

using namespace pldm::utils;

namespace pldmtool
{
namespace helper
{

static const std::map<uint8_t, std::string> genericCompletionCodes{
    {PLDM_SUCCESS, "SUCCESS"},
    {PLDM_ERROR, "ERROR"},
    {PLDM_ERROR_INVALID_DATA, "ERROR_INVALID_DATA"},
    {PLDM_ERROR_INVALID_LENGTH, "ERROR_INVALID_LENGTH"},
    {PLDM_ERROR_NOT_READY, "ERROR_NOT_READY"},
    {PLDM_ERROR_UNSUPPORTED_PLDM_CMD, "ERROR_UNSUPPORTED_PLDM_CMD"},
    {PLDM_ERROR_INVALID_PLDM_TYPE, "ERROR_INVALID_PLDM_TYPE"},
    {PLDM_INVALID_TRANSFER_OPERATION_FLAG, "INVALID_TRANSFER_OPERATION_FLAG"}};

static const std::map<uint8_t, std::string> fwupdateCompletionCodes{
    {PLDM_FWUP_NOT_IN_UPDATE_MODE, "NOT_IN_UPDATE_MODE"},
    {PLDM_FWUP_ALREADY_IN_UPDATE_MODE, "ALREADY_IN_UPDATE_MODE"},
    {PLDM_FWUP_DATA_OUT_OF_RANGE, "DATA_OUT_OF_RANGE"},
    {PLDM_FWUP_INVALID_TRANSFER_LENGTH, "INVALID_TRANSFER_LENGTH"},
    {PLDM_FWUP_INVALID_STATE_FOR_COMMAND, "INVALID_STATE_FOR_COMMAND"},
    {PLDM_FWUP_INCOMPLETE_UPDATE, "INCOMPLETE_UPDATE"},
    {PLDM_FWUP_BUSY_IN_BACKGROUND, "BUSY_IN_BACKGROUND"},
    {PLDM_FWUP_CANCEL_PENDING, "CANCEL_PENDING"},
    {PLDM_FWUP_COMMAND_NOT_EXPECTED, "COMMAND_NOT_EXPECTED"},
    {PLDM_FWUP_RETRY_REQUEST_FW_DATA, "RETRY_REQUEST_FW_DATA"},
    {PLDM_FWUP_UNABLE_TO_INITIATE_UPDATE, "UNABLE_TO_INITIATE_UPDATE"},
    {PLDM_FWUP_ACTIVATION_NOT_REQUIRED, "ACTIVATION_NOT_REQUIRED"},
    {PLDM_FWUP_SELF_CONTAINED_ACTIVATION_NOT_PERMITTED,
     "SELF_CONTAINED_ACTIVATION_NOT_PERMITTED"},
    {PLDM_FWUP_NO_DEVICE_METADATA, "NO_DEVICE_METADATA"},
    {PLDM_FWUP_RETRY_REQUEST_UPDATE, "RETRY_REQUEST_UPDATE"},
    {PLDM_FWUP_NO_PACKAGE_DATA, "NO_PACKAGE_DATA"},
    {PLDM_FWUP_INVALID_TRANSFER_HANDLE, "INVALID_TRANSFER_HANDLE"},
    {PLDM_FWUP_INVALID_TRANSFER_OPERATION_FLAG,
     "INVALID_TRANSFER_OPERATION_FLAG"},
    {PLDM_FWUP_ACTIVATE_PENDING_IMAGE_NOT_PERMITTED,
     "ACTIVATE_PENDING_IMAGE_NOT_PERMITTED"},
    {PLDM_FWUP_PACKAGE_DATA_ERROR, "PACKAGE_DATA_ERROR"}};

void fillCompletionCode(uint8_t completionCode, ordered_json& data,
                        uint8_t pldmType)
{
    // Check generic completion codes first for all PLDM types
    auto it = genericCompletionCodes.find(completionCode);
    if (it != genericCompletionCodes.end())
    {
        data["CompletionCode"] = it->second;
        return;
    }

    // If not a generic code, check type-specific codes
    switch (pldmType)
    {
        case PLDM_FWUP:
        {
            auto typeIt = fwupdateCompletionCodes.find(completionCode);
            if (typeIt != fwupdateCompletionCodes.end())
            {
                data["CompletionCode"] = typeIt->second;
                return;
            }
            break;
        }
    }

    data["CompletionCode"] = "UNKNOWN_COMPLETION_CODE";
}

void CommandInterface::exec()
{
    instanceId = instanceIdDb.next(mctp_eid);
    auto [rc, requestMsg] = createRequestMsg();
    if (rc != PLDM_SUCCESS)
    {
        instanceIdDb.free(mctp_eid, instanceId);
        std::cerr << "Failed to encode request message for " << pldmType << ":"
                  << commandName << " rc = " << rc << "\n";
        return;
    }

    std::vector<uint8_t> responseMsg;
    rc = pldmSendRecv(requestMsg, responseMsg);

    if (rc != PLDM_SUCCESS)
    {
        instanceIdDb.free(mctp_eid, instanceId);
        std::cerr << "pldmSendRecv: Failed to receive RC = " << rc << "\n";
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

        rc =
            pldmTransport.sendRecvMsg(tid, requestMsg.data(), requestMsg.size(),
                                      responseMessage, responseMessageSize);
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
        std::cerr << "failed to pldm send recv error rc " << rc << std::endl;
    }

    return rc;
}
} // namespace helper
} // namespace pldmtool
