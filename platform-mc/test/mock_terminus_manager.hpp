#pragma once

#include "platform-mc/terminus_manager.hpp"

#include <queue>

#include <gmock/gmock.h>

namespace pldm
{
namespace platform_mc
{

class MockTerminusManager : public TerminusManager
{
  public:
    MockTerminusManager(
        sdeventplus::Event& event,
        requester::Handler<requester::Request>& handler,
        pldm::InstanceIdDb& instanceIdDb,
        std::map<pldm_tid_t, std::shared_ptr<Terminus>>& termini,
        Manager* manager) :
        TerminusManager(event, handler, instanceIdDb, termini, LOCAL_EID,
                        manager)
    {}

    exec::task<int> SendRecvPldmMsgOverMctp(mctp_eid_t /*eid*/,
                                            Request& /*request*/,
                                            const pldm_msg** responseMsg,
                                            size_t* responseLen) override
    {
        if (responseMsgs.empty() || responseMsg == nullptr ||
            responseLen == nullptr)
        {
            co_return PLDM_ERROR;
        }

        *responseMsg = responseMsgs.front();
        *responseLen = responseLens.front() - sizeof(pldm_msg_hdr);

        responseMsgs.pop();
        responseLens.pop();
        co_return PLDM_SUCCESS;
    }

    int enqueueResponse(pldm_msg* responseMsg, size_t responseLen)
    {
        if (responseMsg == nullptr)
        {
            return PLDM_ERROR_INVALID_DATA;
        }

        if (responseLen <= sizeof(pldm_msg_hdr))
        {
            return PLDM_ERROR_INVALID_LENGTH;
        }

        responseMsgs.push(responseMsg);
        responseLens.push(responseLen);
        return PLDM_SUCCESS;
    }

    int clearQueuedResponses()
    {
        while (!responseMsgs.empty())
        {
            responseMsgs.pop();
            responseLens.pop();
        }
        return PLDM_SUCCESS;
    }

    std::queue<pldm_msg*> responseMsgs;
    std::queue<size_t> responseLens;
};

} // namespace platform_mc
} // namespace pldm
