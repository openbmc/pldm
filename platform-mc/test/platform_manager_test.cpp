#include "common/instance_id.hpp"
#include "mock_terminus_manager.hpp"
#include "platform-mc/platform_manager.hpp"
#include "test/test_instance_id.hpp"

#include <sdeventplus/event.hpp>

#include <gtest/gtest.h>

using namespace pldm::platform_mc;
using namespace std::chrono;

class PlatformManagerTest : public testing::Test
{
  protected:
    PlatformManagerTest() :
        bus(pldm::utils::DBusHandler::getBus()),
        event(sdeventplus::Event::get_default()), instanceIdDb(),
        reqHandler(pldmTransport, event, instanceIdDb, false, seconds(1), 2,
                   milliseconds(100)),
        mockTerminusManager(event, reqHandler, instanceIdDb, termini, nullptr),
        platformManager(mockTerminusManager, termini)
    {}

    PldmTransport* pldmTransport = nullptr;
    sdbusplus::bus_t& bus;
    sdeventplus::Event event;
    TestInstanceIdDb instanceIdDb;
    pldm::requester::Handler<pldm::requester::Request> reqHandler;
    MockTerminusManager mockTerminusManager;
    PlatformManager platformManager;
    std::map<pldm::tid_t, std::shared_ptr<Terminus>> termini;
};

TEST_F(PlatformManagerTest, initTerminusTest)
{
    // Add terminus
    auto mappedTid = mockTerminusManager.mapTid(pldm::MctpInfo(1, "", "", 1, ""));
    auto tid = mappedTid.value();
    termini[tid] =
        std::make_shared<Terminus>(tid, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);
    auto terminus = termini[tid];

    // queue dummy eventMessageBufferSize response
    const size_t eventMessageBufferSizeRespLen = 1;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + eventMessageBufferSizeRespLen>
        eventMessageBufferSizeResp{0x0, 0x02, 0x0d, PLDM_ERROR};
    auto rc = mockTerminusManager.enqueueResponse(
        (pldm_msg*)eventMessageBufferSizeResp.data(),
        sizeof(eventMessageBufferSizeResp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    // queue dummy eventMessageSupported response
    const size_t eventMessageSupportedLen = 1;
    PLDM_GET_PDR_REPOSITORY_INFO_RESP_BYTES;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + eventMessageSupportedLen>
        eventMessageSupportedResp{0x0, 0x02, 0x0c, PLDM_ERROR};
    rc = mockTerminusManager.enqueueResponse(
        (pldm_msg*)eventMessageSupportedResp.data(),
        sizeof(eventMessageSupportedResp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    // queue getPDRRepositoryInfo response
    const size_t getPDRRepositoryInfoLen =
        PLDM_GET_PDR_REPOSITORY_INFO_RESP_BYTES;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + getPDRRepositoryInfoLen>
        getPDRRepositoryInfoResp{
            0x0, 0x02, 0x50, PLDM_SUCCESS,
            0x0,                                     // repositoryState
            0x0, 0x0,  0x0,  0x0,          0x0, 0x0, 0x0,
            0x0, 0x0,  0x0,  0x0,          0x0, 0x0, // updateTime
            0x0, 0x0,  0x0,  0x0,          0x0, 0x0, 0x0,
            0x0, 0x0,  0x0,  0x0,          0x0, 0x0, // OEMUpdateTime
            1,   0x0,  0x0,  0x0,                    // recordCount
            0x0, 0x1,  0x0,  0x0,                    // repositorySize
            59,  0x0,  0x0,  0x0,                    // largestRecordSize
            0x0 // dataTransferHandleTimeout
        };
    rc = mockTerminusManager.enqueueResponse(
        (pldm_msg*)getPDRRepositoryInfoResp.data(),
        sizeof(getPDRRepositoryInfoResp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    // queue getPDR responses
    const size_t getPdrRespLen = 81;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + getPdrRespLen> getPdrResp{
        0x0, 0x02, 0x51, PLDM_SUCCESS, 0x0, 0x0, 0x0, 0x0, // nextRecordHandle
        0x0, 0x0, 0x0, 0x0, // nextDataTransferHandle
        0x5,                // transferFlag
        69, 0x0,            // responseCount
        // numeric Sensor PDR
        0x1, 0x0, 0x0,
        0x0,                     // record handle
        0x1,                     // PDRHeaderVersion
        PLDM_NUMERIC_SENSOR_PDR, // PDRType
        0x0,
        0x0,                     // recordChangeNumber
        PLDM_PDR_NUMERIC_SENSOR_PDR_FIXED_LENGTH +
            PLDM_PDR_NUMERIC_SENSOR_PDR_VARIED_SENSOR_DATA_SIZE_MIN_LENGTH +
            PLDM_PDR_NUMERIC_SENSOR_PDR_VARIED_RANGE_FIELD_MIN_LENGTH,
        0,                             // dataLength
        0,
        0,                             // PLDMTerminusHandle
        0x1,
        0x0,                           // sensorID=1
        120,
        0,                             // entityType=Power Supply(120)
        1,
        0,                             // entityInstanceNumber
        0x1,
        0x0,                           // containerID=1
        PLDM_NO_INIT,                  // sensorInit
        false,                         // sensorAuxiliaryNamesPDR
        PLDM_SENSOR_UNIT_DEGRESS_C,    // baseUint(2)=degrees C
        1,                             // unitModifier = 1
        0,                             // rateUnit
        0,                             // baseOEMUnitHandle
        0,                             // auxUnit
        0,                             // auxUnitModifier
        0,                             // auxRateUnit
        0,                             // rel
        0,                             // auxOEMUnitHandle
        true,                          // isLinear
        PLDM_SENSOR_DATA_SIZE_UINT8,   // sensorDataSize
        0, 0, 0xc0,
        0x3f,                          // resolution=1.5
        0, 0, 0x80,
        0x3f,                          // offset=1.0
        0,
        0,                             // accuracy
        0,                             // plusTolerance
        0,                             // minusTolerance
        2,                             // hysteresis
        0,                             // supportedThresholds
        0,                             // thresholdAndHysteresisVolatility
        0, 0, 0x80,
        0x3f,                          // stateTransistionInterval=1.0
        0, 0, 0x80,
        0x3f,                          // updateInverval=1.0
        255,                           // maxReadable
        0,                             // minReadable
        PLDM_RANGE_FIELD_FORMAT_UINT8, // rangeFieldFormat
        0,                             // rangeFieldsupport
        0,                             // nominalValue
        0,                             // normalMax
        0,                             // normalMin
        0,                             // warningHigh
        0,                             // warningLow
        0,                             // criticalHigh
        0,                             // criticalLow
        0,                             // fatalHigh
        0                              // fatalLow
    };
    rc = mockTerminusManager.enqueueResponse((pldm_msg*)getPdrResp.data(),
                                             sizeof(getPdrResp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    platformManager.initTerminus();
    EXPECT_EQ(true, terminus->initalized);
    EXPECT_EQ(1, terminus->pdrs.size());
    EXPECT_EQ(1, terminus->numericSensors.size());
}

TEST_F(PlatformManagerTest, negativeInitTerminusTest1)
{
    // terminus doesn't Type2 support
    auto mappedTid = mockTerminusManager.mapTid(pldm::MctpInfo(1, "", "", 1, ""));
    auto tid = mappedTid.value();
    termini[tid] = std::make_shared<Terminus>(tid, 1 << PLDM_BASE);
    auto terminus = termini[tid];

    platformManager.initTerminus();
    EXPECT_EQ(true, terminus->initalized);
    EXPECT_EQ(0, terminus->pdrs.size());
    EXPECT_EQ(0, terminus->numericSensors.size());
}

TEST_F(PlatformManagerTest, negativeInitTerminusTest2)
{
    // terminus responses error
    auto mappedTid = mockTerminusManager.mapTid(pldm::MctpInfo(1, "", "", 1, ""));
    auto tid = mappedTid.value();
    termini[tid] =
        std::make_shared<Terminus>(tid, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);
    auto terminus = termini[tid];

    // queue getPDRRepositoryInfo response cc=PLDM_ERROR
    const size_t getPDRRepositoryInfoLen = 1;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + getPDRRepositoryInfoLen>
        getPDRRepositoryInfoResp{0x0, 0x02, 0x50, PLDM_ERROR};
    auto rc = mockTerminusManager.enqueueResponse(
        (pldm_msg*)getPDRRepositoryInfoResp.data(),
        sizeof(getPDRRepositoryInfoResp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    // queue getPDR responses cc=PLDM_ERROR
    const size_t getPdrRespLen = 1;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + getPdrRespLen> getPdrResp{
        0x0, 0x02, 0x51, PLDM_ERROR};
    rc = mockTerminusManager.enqueueResponse((pldm_msg*)getPdrResp.data(),
                                             sizeof(getPdrResp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    platformManager.initTerminus();
    EXPECT_EQ(true, terminus->initalized);
    EXPECT_EQ(0, terminus->pdrs.size());
    EXPECT_EQ(0, terminus->numericSensors.size());
}
