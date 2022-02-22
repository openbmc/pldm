#include "libpldm/base.h"

#include "platform-mc/manager.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/mctp_endpoint_discovery.hpp"

#include <sdbusplus/timer.hpp>
#include <sdeventplus/event.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace std::chrono;

using ::testing::AtLeast;
using ::testing::Between;
using ::testing::Exactly;
using ::testing::NiceMock;
using ::testing::Return;

class TerminusManagerTest : public testing::Test
{
  protected:
    TerminusManagerTest() :
        bus(pldm::utils::DBusHandler::getBus()),
        event(sdeventplus::Event::get_default()),
        dbusImplRequester(bus, "/xyz/openbmc_project/pldm"),
        reqHandler(fd, event, dbusImplRequester, false, 90000, seconds(1), 2,
                   milliseconds(100)),
        terminusManager(event, reqHandler, dbusImplRequester, terminuses,
                        nullptr)
    {}

    int fd = -1;
    sdbusplus::bus::bus& bus;
    sdeventplus::Event event;
    pldm::dbus_api::Requester dbusImplRequester;
    pldm::requester::Handler<pldm::requester::Request> reqHandler;
    pldm::platform_mc::TerminusManager terminusManager;
    std::vector<std::shared_ptr<pldm::platform_mc::Terminus>> terminuses;
};

TEST_F(TerminusManagerTest, mapTIDTest)
{
    uint8_t eid = 1;
    auto tid = terminusManager.getMappedTID(eid);
    EXPECT_EQ(tid, std::nullopt);

    tid = terminusManager.mapTID(eid);
    EXPECT_NE(tid, std::nullopt);

    auto mappedEID = terminusManager.getMappedEID(tid.value());
    EXPECT_EQ(mappedEID.value(), eid);

    auto mappedTID = terminusManager.getMappedTID(eid);
    EXPECT_EQ(mappedTID.value(), tid.value());

    terminusManager.unmapTID(tid.value());
    tid = terminusManager.getMappedTID(eid);
    EXPECT_EQ(tid, std::nullopt);
}