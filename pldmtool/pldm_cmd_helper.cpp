#include "pldm_cmd_helper.hpp"

#include "common/transport.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <libpldm/firmware_update.h>
#include <libpldm/transport.h>
#include <libpldm/transport/af-mctp.h>
#include <libpldm/transport/mctp-demux.h>
#include <linux/mctp.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <systemd/sd-bus.h>

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <cstring>
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

int mctpSockSendRecv(const uint8_t mctpNetworkId, const uint8_t eid,
                     const bool mctpPreAllocTag,
                     const std::vector<uint8_t>& requestMsg,
                     void** responseMessage, size_t* responseMessageSize)
{
    struct sockaddr_mctp_ext addr;
    int sd;
    int rc;
    struct mctp_ioc_tag_ctl ctl = {
        .peer_addr = eid,
        .tag = 0,
        .flags = 0,
    };

    // open AF_MCTP socket
    sd = socket(AF_MCTP, SOCK_DGRAM, 0);
    if (sd < 0)
    {
        rc = -errno;
        std::cerr << "socket(AF_MCTP, SOCK_DGRAM, 0) failed. errnostr = "
                  << strerror(errno) << "\n";
        return rc;
    }

    // We want extended addressing on all received messages
    int val = 1;
    rc = setsockopt(sd, SOL_MCTP, MCTP_OPT_ADDR_EXT, &val, sizeof(val));
    if (rc < 0)
    {
        rc = -errno;
        std::cerr
            << "Kernel does not support MCTP extended addressing. errnostr = "
            << strerror(errno) << "\n";
        close(sd);
        return rc;
    }

    // prepare the request to be sent
    memset(&addr, 0, sizeof(addr));
    addr.smctp_base.smctp_family = AF_MCTP;
    addr.smctp_base.smctp_network = mctpNetworkId;
    addr.smctp_base.smctp_addr.s_addr = eid;
    addr.smctp_base.smctp_type = requestMsg[0];
    if (mctpPreAllocTag)
    {
        // preallocate a tag if needed
        rc = ioctl(sd, SIOCMCTPALLOCTAG, &ctl);
        if (rc)
        {
            rc = -errno;
            std::cerr << "ioctl(SIOCMCTPALLOCTAG) failed. errnostr = "
                      << strerror(errno) << "\n";
            close(sd);
            return rc;
        }
    } // preAllocTag

    // send the MCTP message
    addr.smctp_base.smctp_tag = MCTP_TAG_OWNER | ctl.tag;
    rc = sendto(sd, &requestMsg[1], requestMsg.size() - 1, 0,
                (struct sockaddr*)&addr, sizeof(struct sockaddr_mctp));
    if (rc < 0)
    {
        rc = -errno;
        std::cerr << "sendto(AF_MCTP) failed. errnostr = " << strerror(errno)
                  << "\n";
        close(sd);
        return rc;
    }

    // wait for for the response from the MCTP Endpoint
    // Instance ID expiration interval (MT4) - after which the instance ID
    // will be reused. For PCIe binding this timeout is 5 seconds.
    const int MCTP_INST_ID_EXPIRATION_INTERVAL_MT4 = 5;
    struct pollfd pollfd;
    pollfd.fd = sd;
    pollfd.events = POLLIN;
    rc = poll(&pollfd, 1, MCTP_INST_ID_EXPIRATION_INTERVAL_MT4 * 1000);
    if (rc < 0)
    {
        std::cerr << "poll(AF_MCTP, 5000) failed. errnostr = "
                  << strerror(errno) << "\n";
        close(sd);
        return rc;
    }
    else if (rc == 0)
    {
        // poll() timed out
        std::cerr << "Timeout(5s): No response from the endpoint\n";
        close(sd);
        return rc;
    }
    else
    {
        // data on the socket
        // take a PEEK at the socket to know how many bytes to read
        int respLen = recvfrom(sd, NULL, 0, MSG_PEEK | MSG_TRUNC, NULL, 0);
        if (respLen < 0)
        {
            rc = -errno;
            std::cerr << "recvfrom(MSG_PEEK | MSG_TRUNC)failed. errnostr = "
                      << strerror(errno) << "\n";
            close(sd);
            return rc;
        }

        // read the received data
        struct sockaddr_mctp retAddr;
        socklen_t addrlen = sizeof(retAddr);
        memset(&retAddr, 0x0, sizeof(retAddr));
        uint8_t* respBuf;
        respBuf = (uint8_t*)malloc(respLen);
        int rcvdByteCount = recvfrom(sd, respBuf, respLen, MSG_TRUNC,
                                     (struct sockaddr*)&retAddr, &addrlen);
        if (rcvdByteCount < 0)
        {
            rc = -errno;
            std::cerr << "recvfrom(): DATA failed: " << strerror(errno) << "\n";
            free(respBuf);
            close(sd);
            return rc;
        }
        if (mctpPreAllocTag)
        {
            // drop the preallocated msg tag
            rc = ioctl(sd, SIOCMCTPDROPTAG, &ctl);
            if (rc)
            {
                rc = -errno;
                std::cerr << "ioctl(SIOCMCTPDROPTAG) failed. errnostr = "
                          << strerror(errno) << "\n";
                free(respBuf);
                close(sd);
                return rc;
            }
        }
        *responseMessageSize = rcvdByteCount;
        *responseMessage = (void*)respBuf;
    }

    close(sd);
    return 0;
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
    if ((CommandInterface::pldmType == "raw") ||
        (CommandInterface::pldmType == "mctpRaw"))
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

        if (CommandInterface::pldmType != "mctpRaw")
        {
            rc = pldmTransport.sendRecvMsg(tid, requestMsg.data(),
                                           requestMsg.size(), responseMessage,
                                           responseMessageSize);
            if (rc)
            {
                std::cerr << "[" << unsigned(retry)
                          << "] pldm_send_recv error rc " << rc << std::endl;
                retry++;
                continue;
            }
        }
        else
        {
            rc = mctpSockSendRecv(mctpNetworkId, mctp_eid, mctpPreAllocTag,
                                  requestMsg, &responseMessage,
                                  &responseMessageSize);
            if (rc)
            {
                std::cerr << "[" << unsigned(retry)
                          << "] mctpSockSendRecv() error rc " << rc
                          << std::endl;
                retry++;
                continue;
            }
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
