#include "libpldm/base.h"

#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "mock_terminus_manager.hpp"
#include "platform-mc/terminus_manager.hpp"
#include "requester/handler.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "requester/request.hpp"
#include "test/test_instance_id.hpp"

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
        event(sdeventplus::Event::get_default()), instanceIdDb(),
        reqHandler(pldmTransport, event, instanceIdDb, false, seconds(1), 2,
                   milliseconds(100)),
        terminusManager(event, reqHandler, instanceIdDb, termini, 0x0, nullptr),
        mockTerminusManager(event, reqHandler, instanceIdDb, termini, nullptr)
    {}

    PldmTransport* pldmTransport = nullptr;
    sdbusplus::bus_t& bus;
    sdeventplus::Event event;
    TestInstanceIdDb instanceIdDb;
    pldm::requester::Handler<pldm::requester::Request> reqHandler;
    pldm::platform_mc::TerminusManager terminusManager;
    pldm::platform_mc::MockTerminusManager mockTerminusManager;
    std::map<pldm_tid_t, std::shared_ptr<pldm::platform_mc::Terminus>> termini;
};

TEST_F(TerminusManagerTest, mapTidTest)
{
    pldm::MctpInfo mctpInfo1(1, "", "", 0);

    auto mappedTid1 = terminusManager.mapTid(mctpInfo1);
    EXPECT_NE(mappedTid1, std::nullopt);

    auto tid1 = terminusManager.toTid(mctpInfo1);
    EXPECT_NE(tid1, std::nullopt);

    auto mctpInfo2 = terminusManager.toMctpInfos(tid1.value());
    EXPECT_EQ(mctpInfo1, mctpInfo2->back());

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
    EXPECT_NE(mappedTid3, std::nullopt);

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
    auto mappedEid = terminusManager.toMctpInfos(0);
    EXPECT_EQ(mappedEid, std::nullopt);

    // look up reserved TID(0xff)
    mappedEid = terminusManager.toMctpInfos(0xff);
    EXPECT_EQ(mappedEid, std::nullopt);

    // unmap one medium interface of two from TID 1
    auto ret = terminusManager.unmapMctpInfo(1, m3);
    EXPECT_EQ(ret, true);

    // unmap reserved TID(0)
    ret = terminusManager.unmapMctpInfo(1, m4);
    EXPECT_EQ(ret, false);

    mappedEid = terminusManager.toMctpInfos(1);
    EXPECT_NE(mappedEid, std::nullopt);

    // look up an unmapped TID
    ret = terminusManager.unmapTid(1);
    EXPECT_EQ(ret, true);
    mappedEid = terminusManager.toMctpInfos(1);
    EXPECT_EQ(mappedEid, std::nullopt);

    // unmap reserved TID(0)
    ret = terminusManager.unmapMctpInfo(1, m4);
    EXPECT_EQ(ret, false);

    // unmap reserved TID(0)
    ret = terminusManager.unmapTid(0);
    EXPECT_EQ(ret, false);

    // unmap reserved TID(0)
    ret = terminusManager.unmapTid(0xff);
    EXPECT_EQ(ret, false);
}

TEST_F(TerminusManagerTest, discoverMctpTerminusTest)
{
    const size_t getTidRespLen = 2;
    const size_t setTidRespLen = 1;
    const size_t getPldmTypesRespLen = 9;

    // 0.discover a mctp list
    auto rc = mockTerminusManager.clearQueuedResponses();
    EXPECT_EQ(rc, PLDM_SUCCESS);

    std::array<uint8_t, sizeof(pldm_msg_hdr) + getTidRespLen> getTidResp0{
        0x00, 0x02, 0x02, 0x00, 0x00};
    rc = mockTerminusManager.enqueueResponse((pldm_msg*)getTidResp0.data(),
                                             sizeof(getTidResp0));
    EXPECT_EQ(rc, PLDM_SUCCESS);
    std::array<uint8_t, sizeof(pldm_msg_hdr) + setTidRespLen> setTidResp0{
        0x00, 0x02, 0x01, 0x00};
    rc = mockTerminusManager.enqueueResponse((pldm_msg*)setTidResp0.data(),
                                             sizeof(setTidResp0));
    EXPECT_EQ(rc, PLDM_SUCCESS);
    std::array<uint8_t, sizeof(pldm_msg_hdr) + getPldmTypesRespLen>
        getPldmTypesResp0{0x00, 0x02, 0x04, 0x00, 0x01, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    rc = mockTerminusManager.enqueueResponse(
        (pldm_msg*)getPldmTypesResp0.data(), sizeof(getPldmTypesResp0));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    pldm::MctpInfos mctpInfos{};
    pldm::MctpInfo mctpInfo1(12, "", "", 1);
    mctpInfos.emplace_back(mctpInfo1);
    mockTerminusManager.discoverMctpTerminus(mctpInfos);
    EXPECT_EQ(1, termini.size());

    // 1.discover the same mctp list again
    rc = mockTerminusManager.clearQueuedResponses();
    EXPECT_EQ(rc, PLDM_SUCCESS);

    std::array<uint8_t, sizeof(pldm_msg_hdr) + getTidRespLen> getTidResp1{
        0x00, 0x02, 0x02, 0x00, 0x01};
    rc = mockTerminusManager.enqueueResponse((pldm_msg*)getTidResp1.data(),
                                             sizeof(getTidResp1));
    EXPECT_EQ(rc, PLDM_SUCCESS);
    rc = mockTerminusManager.enqueueResponse((pldm_msg*)setTidResp0.data(),
                                             sizeof(setTidResp0));
    EXPECT_EQ(rc, PLDM_SUCCESS);
    rc = mockTerminusManager.enqueueResponse(
        (pldm_msg*)getPldmTypesResp0.data(), sizeof(getPldmTypesResp0));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    mockTerminusManager.discoverMctpTerminus(mctpInfos);
    EXPECT_EQ(1, termini.size());

    // 2.discover an empty mctp list
    rc = mockTerminusManager.clearQueuedResponses();
    EXPECT_EQ(rc, PLDM_SUCCESS);

    mockTerminusManager.removeMctpTerminus(mctpInfos);
    EXPECT_EQ(0, termini.size());
}

TEST_F(TerminusManagerTest, negativeDiscoverMctpTerminusTest)
{
    const size_t getTidRespLen = 2;
    const size_t setTidRespLen = 1;
    const size_t getPldmTypesRespLen = 9;

    // 0.terminus returns reserved tid
    std::array<uint8_t, sizeof(pldm_msg_hdr) + getTidRespLen> getTidResp0{
        0x00, 0x02, 0x02, 0x00, PLDM_TID_RESERVED};
    auto rc = mockTerminusManager.enqueueResponse((pldm_msg*)getTidResp0.data(),
                                                  sizeof(getTidResp0));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    pldm::MctpInfos mctpInfos{};
    mctpInfos.emplace_back(pldm::MctpInfo(12, "", "", 1));
    mockTerminusManager.discoverMctpTerminus(mctpInfos);
    EXPECT_EQ(0, termini.size());

    // 1.terminus return cc=pldm_error for set tid
    std::array<uint8_t, sizeof(pldm_msg_hdr) + getTidRespLen> getTidResp1{
        0x00, 0x02, 0x02, 0x00, 0x00};
    std::array<uint8_t, sizeof(pldm_msg_hdr) + setTidRespLen> setTidResp1{
        0x00, 0x02, 0x01, PLDM_ERROR};

    rc = mockTerminusManager.enqueueResponse((pldm_msg*)getTidResp1.data(),
                                             sizeof(getTidResp1));
    EXPECT_EQ(rc, PLDM_SUCCESS);
    rc = mockTerminusManager.enqueueResponse((pldm_msg*)setTidResp1.data(),
                                             sizeof(setTidResp1));
    EXPECT_EQ(rc, PLDM_SUCCESS);
    mockTerminusManager.removeMctpTerminus(mctpInfos);
    EXPECT_EQ(0, termini.size());

    // 2.terminus return cc=unsupported_pldm_cmd for set tid cmd and return
    // cc=pldm_error for get pldm types cmd
    std::array<uint8_t, sizeof(pldm_msg_hdr) + getTidRespLen> getTidResp2{
        0x00, 0x02, 0x02, 0x00, 0x00};
    std::array<uint8_t, sizeof(pldm_msg_hdr) + setTidRespLen> setTidResp2{
        0x00, 0x02, 0x01, PLDM_ERROR_UNSUPPORTED_PLDM_CMD};
    std::array<uint8_t, sizeof(pldm_msg_hdr) + getPldmTypesRespLen>
        getPldmTypesResp2{0x00, 0x02, 0x04, PLDM_ERROR, 0x01, 0x00,
                          0x00, 0x00, 0x00, 0x00,       0x00, 0x00};
    rc = mockTerminusManager.enqueueResponse((pldm_msg*)getTidResp2.data(),
                                             sizeof(getTidResp2));
    EXPECT_EQ(rc, PLDM_SUCCESS);
    rc = mockTerminusManager.enqueueResponse((pldm_msg*)setTidResp2.data(),
                                             sizeof(setTidResp2));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    rc = mockTerminusManager.enqueueResponse(
        (pldm_msg*)getPldmTypesResp2.data(), sizeof(getPldmTypesResp2));
    EXPECT_EQ(rc, PLDM_SUCCESS);
    mockTerminusManager.removeMctpTerminus(mctpInfos);
    EXPECT_EQ(0, termini.size());
}
