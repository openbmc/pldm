#pragma once

#include "requester/mctp_endpoint_discovery.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace pldm
{

class MockManager : public pldm::MctpDiscoveryHandlerIntf
{
  public:
    MOCK_METHOD(void, handleMCTPEndpoints, (const MctpInfos& mctpInfos),
                (override));
};

} // namespace pldm
