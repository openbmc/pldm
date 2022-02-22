#include "libpldm/base.h"

#include "common/types.hpp"
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
        terminusManager(event, reqHandler, dbusImplRequester, termini, nullptr)
    {}

    int fd = -1;
    sdbusplus::bus_t& bus;
    sdeventplus::Event event;
    pldm::dbus_api::Requester dbusImplRequester;
    pldm::requester::Handler<pldm::requester::Request> reqHandler;
    pldm::platform_mc::TerminusManager terminusManager;
    std::map<mctp_eid_t, std::shared_ptr<pldm::platform_mc::Terminus>> termini;
};

TEST_F(TerminusManagerTest, mapTidTest)
{
    pldm::MctpInfo mctpInfo1(0, 1, "");

    auto tid1 = terminusManager.toTid(mctpInfo1);
    EXPECT_EQ(tid1, std::nullopt);

    tid1 = terminusManager.mapTid(mctpInfo1);
    EXPECT_NE(tid1, std::nullopt);

    auto mctpInfo2 = terminusManager.toMctpInfo(tid1.value());
    EXPECT_EQ(mctpInfo1, mctpInfo2.value());

    terminusManager.unmapTid(tid1.value());

    tid1 = terminusManager.toTid(mctpInfo1);
    EXPECT_EQ(tid1, std::nullopt);
}

TEST_F(TerminusManagerTest, negativeMapTidTest)
{
    // map null EID(0) to TID
    pldm::MctpInfo m0(0, 0, "");
    auto mappedTid = terminusManager.mapTid(m0);
    EXPECT_EQ(mappedTid, std::nullopt);

    // map broadcast EID(0xff) to TID
    pldm::MctpInfo m1(0, 0xff, "");
    mappedTid = terminusManager.mapTid(m1);
    EXPECT_EQ(mappedTid, std::nullopt);

    // look up an unmapped MctpInfo to TID
    pldm::MctpInfo m2(0, 1, "");
    mappedTid = terminusManager.toTid(m2);
    EXPECT_EQ(mappedTid, std::nullopt);

    // look up reserved TID(0)
    auto mappedEid = terminusManager.toMctpInfo(0);
    EXPECT_EQ(mappedEid, std::nullopt);

    // look up reserved TID(0xff)
    mappedEid = terminusManager.toMctpInfo(0xff);
    EXPECT_EQ(mappedEid, std::nullopt);

    // look up an unmapped TID
    mappedEid = terminusManager.toMctpInfo(1);
    EXPECT_EQ(mappedEid, std::nullopt);

    // map two mctpInfo with same EID but different network Id
    pldm::MctpInfo m3(1, 12, "");
    pldm::MctpInfo m4(2, 12, "");
    auto mappedTid3 = terminusManager.mapTid(m3);
    auto mappedTid4 = terminusManager.mapTid(m4);
    EXPECT_NE(mappedTid3.value(), mappedTid4.value());

    // map same mctpInfo twice
    pldm::MctpInfo m5(3, 12, "");
    auto mappedTid5 = terminusManager.mapTid(m5);
    auto mappedTid6 = terminusManager.mapTid(m5);
    EXPECT_EQ(mappedTid5.value(), mappedTid6.value());
}