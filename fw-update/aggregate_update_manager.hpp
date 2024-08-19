#pragma once

#include "update_manager.hpp"

namespace pldm::fw_update
{

class AggregateUpdateManager final :
    public std::map<DeviceIdentifier,
                    std::tuple<DescriptorMap, ComponentInfoMap,
                               std::shared_ptr<UpdateManager>>>,
    public UpdateManager
{
  public:
    explicit AggregateUpdateManager(
        Event& event,
        pldm::requester::Handler<pldm::requester::Request>& handler,
        InstanceIdDb& instanceIdDb, const DescriptorMap& descriptorMap,
        const ComponentInfoMap& componentInfoMap) :
        UpdateManager(event, handler, instanceIdDb, descriptorMap,
                      componentInfoMap),
        event(event), handler(handler), instanceIdDb(instanceIdDb)
    {}

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
            for (auto& [deviceIdentifier, value] : *this)
            {
                auto& [descriptorMap, componentInfoMap, manager] = value;
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

    using std::map<
        DeviceIdentifier,
        std::tuple<DescriptorMap, ComponentInfoMap,
                   std::shared_ptr<UpdateManager>>>::insert_or_assign;

    void insert_or_assign(const DeviceIdentifier& deviceIdentifier,
                          DescriptorMap&& descriptorMap,
                          ComponentInfoMap&& componentInfoMap)
    {
        (*this)[deviceIdentifier] = std::make_tuple(
            std::move(descriptorMap), std::move(componentInfoMap),
            std::make_shared<UpdateManager>(
                event, handler, instanceIdDb,
                std::get<DescriptorMap>(at(deviceIdentifier)),
                std::get<ComponentInfoMap>(at(deviceIdentifier))));
    }

  private:
    Event& event;
    pldm::requester::Handler<pldm::requester::Request>& handler;
    InstanceIdDb& instanceIdDb;
};

} // namespace pldm::fw_update
