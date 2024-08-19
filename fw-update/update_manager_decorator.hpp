#pragma once

#include "update_manager.hpp"
#include <memory>

namespace pldm::fw_update
{

class UpdateManagerDecorator : public UpdateManagerInf
{
  public:
    UpdateManagerDecorator(const std::shared_ptr<UpdateManagerInf>& updateManager) :
      updateManager(updateManager)
    {}

    Response handleRequest(mctp_eid_t eid, uint8_t command,
                           const pldm_msg* request, size_t reqMsgLen) override
    {
        return updateManager->handleRequest(eid, command, request, reqMsgLen);
    }

  private:
    std::shared_ptr<UpdateManagerInf> updateManager;
};

} // namespace pldm::fw_update
