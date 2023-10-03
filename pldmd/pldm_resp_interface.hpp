#pragma once

#include "common/flight_recorder.hpp"
#include "common/utils.hpp"

#include <sys/socket.h>

#include <phosphor-logging/lg2.hpp>

#include <iostream>
#include <map>
#include <memory>
#include <vector>
using namespace pldm;
using namespace pldm::utils;
using namespace pldm::flightrecorder;

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace response_api
{

/** @class Transport
 *
 *  @brief This class performs the necessary operation in pldm for
 *         responding remote pldm. This class is mostly designed in special case
 *         when pldm need to send reply to host after FILE IO operation
 *         completed.
 */
class Transport
{
  public:
    /** @brief Transport constructor
     */
    Transport() = delete;
    Transport(const Transport&) = delete;
    Transport(int socketFd, bool verbose = false) :
        sockFd(socketFd), verbose(verbose)
    {}

    /** @brief method to send response to remote pldm using transport interface
     * @param response - response of each request
     * @param index - index to delete header from maintained map
     * @returns returns 0 if success else -1
     */
    int sendPLDMRespMsg(Response& response, int index)
    {
        struct iovec iov[2]{};
        struct msghdr msg
        {};

        FlightRecorder::GetInstance().saveRecord(response, true);
        if (verbose)
        {
            printBuffer(Tx, response);
        }

        iov[0].iov_base = &requestMap[index][0];
        iov[0].iov_len = sizeof(requestMap[index][0]) +
                         sizeof(requestMap[index][1]);
        iov[1].iov_base = response.data();
        iov[1].iov_len = response.size();
        msg.msg_iov = iov;
        msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);

        int rc = sendmsg(sockFd, &msg, 0);
        removeHeader(index);
        if (rc < 0)
        {
            rc = errno;
            error("sendto system call failed, errno= {ERRNO}", "ERRNO", rc);
        }
        return rc;
    }

    /** @brief method to acquire one copy of request header into request map.
     * @param reqMsg - overwrite each request header into existing mRequestMsg
     * variable
     */
    void setRequestMsgRef(std::vector<uint8_t>& reqMsg)
    {
        mRequestMsg = reqMsg;
    }

    /** @brief method to store request header into map when DMA data transfer
     * request received.
     */
    int getRequestHeaderIndex()
    {
        int index = getUniqueKey();
        requestMap[index] = mRequestMsg;
        return index;
    }

  private:
    /** @brief method to remove request header after sending response to host
     */
    void removeHeader(int index)
    {
        requestMap.erase(index);
    }

    /** @brief method to generate unique key to store request header into map
     * @returns available nearest key value as integer
     */
    int getUniqueKey()
    {
        int key = 0;
        for (size_t index = 0; index <= requestMap.size(); index++)
        {
            if (requestMap.find(index) == requestMap.end())
            {
                key = index;
                break;
            }
        }
        return key;
    }

  private:
    std::vector<uint8_t> mRequestMsg;
    std::map<int, std::vector<uint8_t>> requestMap;
    int sockFd;
    bool verbose;
};

struct Interfaces
{
    std::unique_ptr<Transport> transport = nullptr;
};

} // namespace response_api

} // namespace pldm
