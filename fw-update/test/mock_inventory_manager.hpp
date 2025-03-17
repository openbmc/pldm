#pragma once

#include "fw-update/inventory_manager.hpp"

#include <gmock/gmock.h>

namespace pldm
{
namespace fw_update
{

class MockInventoryManager : public InventoryManager
{
  public:
    MockInventoryManager(requester::Handler<requester::Request>& handler,
                         InstanceIdDb& instanceIdDb,
                         DescriptorMap& descriptorMap,
                         DownstreamDescriptorMap& downstreamDescriptorMap,
                         ComponentInfoMap& componentInfoMap) :
        InventoryManager(handler, instanceIdDb, descriptorMap,
                         downstreamDescriptorMap, componentInfoMap)
    {}

    MOCK_METHOD(exec::task<int>, queryDownstreamIdentifiers,
                (mctp_eid_t eid, uint32_t dataTransferHandle,
                 enum transfer_op_flag transferOperationFlag),
                (override));

    MOCK_METHOD(exec::task<int>, getDownstreamFirmwareParameters,
                (mctp_eid_t eid, uint32_t dataTransferHandle,
                 enum transfer_op_flag transferOperationFlag),
                (override));
};

} // namespace fw_update
} // namespace pldm
