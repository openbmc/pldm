#pragma once

#include "requester/request.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace pldm
{

namespace platform_mc
{

class MockManager : public Manager
{
  public:
    explicit MockManager(sdeventplus::Event& event,
                     requester::Handler<requester::Request>& handler,
                     pldm::dbus_api::Requester& requester): Manager(event, handler, requester)
    {}

    MOCK_METHOD(void, handleMCTPEndpoints, (const std::vector<mctp_eid_t>& eids), (override));
};

} // namespace platform_mc

} // namespace pldm