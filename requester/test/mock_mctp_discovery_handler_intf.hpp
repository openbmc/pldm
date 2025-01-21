#pragma once

#include "requester/mctp_endpoint_discovery.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace pldm
{

class MockManager : public pldm::MctpDiscoveryHandlerIntf
{
  public:
    MOCK_METHOD(void, handleMctpEndpoints, (const MctpInfos& mctpInfos),
                (override));
    MOCK_METHOD(void, handleRemovedMctpEndpoints, (const MctpInfos& mctpInfos),
                (override));
    MOCK_METHOD(void, updateMctpEndpointAvailability,
                (const MctpInfo& mctpInfo, Availability availability),
                (override));
    MOCK_METHOD(std::optional<mctp_eid_t>, getActiveEidByName,
                (const std::string& terminusName), (override));
};

} // namespace pldm
