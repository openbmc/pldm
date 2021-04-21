#include "oem/ibm/host-bmc/host_lamp_test.hpp"
#include "pldmd/dbus_impl_requester.hpp"

#include <string.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm::led;
using ::testing::_;
using ::testing::Return;

class MockLampTest : public HostLampTest
{
  public:
    MockLampTest(sdbusplus::bus::bus& bus, const std::string& objPath,
                 int mctp_fd, uint8_t mctp_eid, Requester& requester,
                 const pldm_pdr* repo) :
        HostLampTest(bus, objPath, mctp_fd, mctp_eid, requester, repo)
    {}

    MOCK_METHOD(uint16_t, getEffecterID, (), (override));
    MOCK_METHOD(int, setHostStateEffecter, (uint16_t effecterId), (override));
};

TEST(TestLamp, Asserted)
{
    sdbusplus::bus::bus bus = sdbusplus::bus::new_default();
    pldm::dbus_api::Requester dbusImplReq(bus, "/abc/def/pldm");

    MockLampTest lampTest(bus, "/xyz/openbmc_project/led/groups/host_lamp_test",
                          0, 0, dbusImplReq, nullptr);

    EXPECT_CALL(lampTest, getEffecterID())
        .Times(2)
        .WillOnce(Return(0))
        .WillOnce(Return(1));
    EXPECT_CALL(lampTest, setHostStateEffecter(1)).WillOnce(Return(0));

    lampTest.asserted(false);
    EXPECT_EQ(lampTest.getAsserted(), false);

    ASSERT_THROW(lampTest.asserted(true), std::exception);

    lampTest.asserted(true);
    EXPECT_EQ(lampTest.getAsserted(), true);
}