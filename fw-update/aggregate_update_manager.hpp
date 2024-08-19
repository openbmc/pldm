#pragma once

#include "update_manager_decorator.hpp"

namespace pldm::fw_update
{

class AggregateUpdateManager : public UpdateManagerDecorator
{
  public:
    AggregateUpdateManager(const std::shared_ptr<UpdateManagerInf>& updateManager) :
        UpdateManagerDecorator(updateManager)
    {}

    void addUpdateManager(const std::shared_ptr<UpdateManagerInf>& updateManager)
    {
        updateManagers.emplace_back(updateManager);
    }

    Response handleRequest(mctp_eid_t eid, uint8_t command,
                           const pldm_msg* request, size_t reqMsgLen) override
    {
        auto response = UpdateManagerDecorator::handleRequest(
            eid, command, request, reqMsgLen);
        auto responseMsg = reinterpret_cast<pldm_msg*>(response.data());

        if (responseMsg->payload[0] != PLDM_FWUP_COMMAND_NOT_EXPECTED)
        {
            return response;
        }
        else
        {
            for (auto& manager : updateManagers)
            {
                response = manager->handleRequest(eid, command, request,
                                                 reqMsgLen);
                responseMsg = reinterpret_cast<pldm_msg*>(response.data());
                if (responseMsg->payload[0] != PLDM_FWUP_COMMAND_NOT_EXPECTED)
                {
                    return response;
                }
            }

            return response;
        }
    }

  private:
    std::vector<std::shared_ptr<UpdateManagerInf>> updateManagers;
};

} // namespace pldm::fw_update
