#pragma once

#include "requester/mctp_endpoint_discovery.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace pldm
{

class MockManager : public pldm::IMctpDiscoveryHandler
{
  public:
    MOCK_METHOD(void, handleMCTPEndpoints, (const std::vector<mctp_eid_t>& eids), (override));
};

} // namespace pldm