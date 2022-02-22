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
    std::map<pldm::tid_t, std::shared_ptr<pldm::platform_mc::Terminus>> termini;
};

TEST_F(TerminusManagerTest, mapTidTest)
{
    pldm::MctpInfo mctpInfo1(1, "", "", 0);

    auto mappedTid1 = terminusManager.mapTid(mctpInfo1);
    EXPECT_NE(mappedTid1, std::nullopt);

    auto tid1 = terminusManager.toTid(mctpInfo1);
    EXPECT_NE(tid1, std::nullopt);

    auto mctpInfo2 = terminusManager.toMctpInfo(tid1.value());
    EXPECT_EQ(mctpInfo1, mctpInfo2.value());

    auto ret = terminusManager.unmapTid(tid1.value());
    EXPECT_EQ(ret, true);

    tid1 = terminusManager.toTid(mctpInfo1);
    EXPECT_EQ(tid1, std::nullopt);
}

TEST_F(TerminusManagerTest, negativeMapTidTest)
{
    // map null EID(0) to TID
    pldm::MctpInfo m0(0, "", "", 0);
    auto mappedTid = terminusManager.mapTid(m0);
    EXPECT_EQ(mappedTid, std::nullopt);

    // map broadcast EID(0xff) to TID
    pldm::MctpInfo m1(0xff, "", "", 0);
    mappedTid = terminusManager.mapTid(m1);
    EXPECT_EQ(mappedTid, std::nullopt);

    // map EID to tid which has been assigned
    pldm::MctpInfo m2(2, "", "", 1);
    pldm::MctpInfo m3(3, "", "", 1);
    auto mappedTid2 = terminusManager.mapTid(m2);
    auto mappedTid3 = terminusManager.mapTid(m3, mappedTid2.value());
    EXPECT_NE(mappedTid2, std::nullopt);
    EXPECT_EQ(mappedTid3, std::nullopt);

    // map two mctpInfo with same EID but different network Id
    pldm::MctpInfo m4(12, "", "", 1);
    pldm::MctpInfo m5(12, "", "", 2);
    auto mappedTid4 = terminusManager.mapTid(m4);
    auto mappedTid5 = terminusManager.mapTid(m5);
    EXPECT_NE(mappedTid4.value(), mappedTid5.value());

    // map same mctpInfo twice
    pldm::MctpInfo m6(12, "", "", 3);
    auto mappedTid6 = terminusManager.mapTid(m6);
    auto mappedTid6_1 = terminusManager.mapTid(m6);
    EXPECT_EQ(mappedTid6.value(), mappedTid6_1.value());

    // look up an unmapped MctpInfo to TID
    pldm::MctpInfo m7(1, "", "", 0);
    auto mappedTid7 = terminusManager.toTid(m7);
    EXPECT_EQ(mappedTid7, std::nullopt);

    // look up reserved TID(0)
    auto mappedEid = terminusManager.toMctpInfo(0);
    EXPECT_EQ(mappedEid, std::nullopt);

    // look up reserved TID(0xff)
    mappedEid = terminusManager.toMctpInfo(0xff);
    EXPECT_EQ(mappedEid, std::nullopt);

    // look up an unmapped TID
    terminusManager.unmapTid(1);
    mappedEid = terminusManager.toMctpInfo(1);
    EXPECT_EQ(mappedEid, std::nullopt);

    // unmap reserved TID(0)
    auto ret = terminusManager.unmapTid(0);
    EXPECT_EQ(ret, false);

    // unmap reserved TID(0)
    ret = terminusManager.unmapTid(0xff);
    EXPECT_EQ(ret, false);
}
