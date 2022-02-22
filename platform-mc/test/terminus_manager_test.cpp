#include "libpldm/base.h"

#include "platform-mc/terminus_manager.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/handler.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "requester/request.hpp"

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
        terminusManager(event, reqHandler, dbusImplRequester, termini)
    {}

    int fd = -1;
    sdbusplus::bus::bus& bus;
    sdeventplus::Event event;
    pldm::dbus_api::Requester dbusImplRequester;
    pldm::requester::Handler<pldm::requester::Request> reqHandler;
    pldm::platform_mc::TerminusManager terminusManager;
    std::map<mctp_eid_t, std::shared_ptr<pldm::platform_mc::Terminus>> termini;
};

TEST_F(TerminusManagerTest, mapTIDTest)
{
    uint8_t eid = 1;

    auto mappedTid = terminusManager.toTID(eid);
    EXPECT_EQ(mappedTid, std::nullopt);

    auto tid = terminusManager.mapToTID(eid);
    EXPECT_NE(tid, std::nullopt);

    auto mappedEid = terminusManager.toEID(tid.value());
    EXPECT_EQ(mappedEid.value(), eid);

    mappedTid = terminusManager.toTID(eid);
    EXPECT_EQ(mappedTid.value(), tid.value());

    terminusManager.unmapTID(tid.value());
    mappedTid = terminusManager.toTID(eid);
    EXPECT_EQ(mappedTid, std::nullopt);
}