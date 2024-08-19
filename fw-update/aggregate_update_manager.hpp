#pragma once

#include "update_manager.hpp"

namespace pldm::fw_update
{

class AggregateUpdateManager :
    public std::vector<std::shared_ptr<UpdateManager>>,
    public UpdateManager
{
  public:
    using UpdateManager::UpdateManager;

    Response handleRequest(mctp_eid_t eid, uint8_t command,
                           const pldm_msg* request, size_t reqMsgLen) override
    {
        auto response =
            UpdateManager::handleRequest(eid, command, request, reqMsgLen);
        auto responseMsg = new (response.data()) pldm_msg;

        if (responseMsg->payload[0] != PLDM_FWUP_COMMAND_NOT_EXPECTED)
        {
            return response;
        }
        else
        {
            for (auto& manager : *this)
            {
                response =
                    manager->handleRequest(eid, command, request, reqMsgLen);
                responseMsg = new (response.data()) pldm_msg;
                if (responseMsg->payload[0] != PLDM_FWUP_COMMAND_NOT_EXPECTED)
                {
                    return response;
                }
            }

            return response;
        }
    }
};

} // namespace pldm::fw_update
