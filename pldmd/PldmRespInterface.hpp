#pragma once

#include "common/utils.hpp"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <iostream>
#include <map>
#include <memory>
#include <vector>

using namespace pldm::utils;

namespace pldm
{
namespace response_api
{
using Response = std::vector<uint8_t>;
class Transport
{
  public:
    /** @brief Transport constructor
     */
    Transport(int socketFd, bool verbose = false) :
        sockFd(socketFd), verbose(verbose)
    {}

    /** @brief method to send response to host using transport interface
     * @param response - response of each request
     * @param indx - index to delete header from maintained map
     * @returns returns 0 if success else -1
     */
    int sendPLDMRespMsg(Response& response, uint32_t indx)
    {
        struct iovec iov[2]{};
        struct msghdr msg
        {};
        if (verbose)
        {
            printBuffer(Tx, response);
        }
        iov[0].iov_base = &requestMap[indx][0];
        iov[0].iov_len = sizeof(requestMap[indx][0]) +
                         sizeof(requestMap[indx][1]);
        iov[1].iov_base = response.data();
        iov[1].iov_len = response.size();
        msg.msg_iov = iov;
        msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);

        int rc = sendmsg(sockFd, &msg, 0);
        if (rc < 0)
        {
            rc = errno;
            std::cerr << "sendto system call failed, RC= " << errno << "\n";
        }

        removeHeader(indx);
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
        int indx = getUniqueKey();
        requestMap[indx] = mRequestMsg;
        return indx;
    }

  private:
    /** @brief method to remove request header after sending response to host
     */
    void removeHeader(uint32_t indx)
    {
        requestMap.erase(indx);
    }

    /** @brief method to generate unique key to store request header into map
     * @returns available nearest key value as integer
     */
    uint32_t getUniqueKey()
    {
        uint32_t key = 0;
        for (size_t indx = 0; indx <= requestMap.size(); indx++)
        {
            if (requestMap.find(indx) == requestMap.end())
            {
                key = indx;
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
    std::unique_ptr<Transport> transport{};
};

} // namespace response_api

} // namespace pldm
