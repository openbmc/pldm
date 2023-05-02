#include "libpldm/base.h"
#include "libpldm/entity.h"
#include "libpldm/platform.h"

#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "mock_event_manager.hpp"
#include "mock_terminus_manager.hpp"
#include "platform-mc/platform_manager.hpp"
#include "platform-mc/terminus_manager.hpp"
#include "test/test_instance_id.hpp"

#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;

using namespace pldm::platform_mc;

class EventManagerTest : public testing::Test
{
  protected:
    EventManagerTest() :
        bus(pldm::utils::DBusHandler::getBus()),
        event(sdeventplus::Event::get_default()), instanceIdDb(),
        reqHandler(pldmTransport, event, instanceIdDb, false, seconds(1), 2,
                   milliseconds(100)),
        terminusManager(event, reqHandler, instanceIdDb, termini, nullptr),
        eventManager(terminusManager, termini),
        platformManager(terminusManager, termini)
    {}

    PldmTransport* pldmTransport = nullptr;
    sdbusplus::bus::bus& bus;
    sdeventplus::Event event;
    TestInstanceIdDb instanceIdDb;
    pldm::requester::Handler<pldm::requester::Request> reqHandler;
    MockTerminusManager terminusManager;
    MockEventManager eventManager;
    PlatformManager platformManager;
    std::map<pldm::tid_t, std::shared_ptr<Terminus>> termini{};
};

TEST_F(EventManagerTest, processNumericSensorEventTest)
{
#define SENSOR_READING 50
#define WARNING_HIGH 45
    pldm::tid_t tid = 1;
    termini[tid] =
        std::make_shared<Terminus>(tid, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);
    std::vector<uint8_t> pdr1{
        0x1,
        0x0,
        0x0,
        0x0,                         // record handle
        0x1,                         // PDRHeaderVersion
        PLDM_NUMERIC_SENSOR_PDR,     // PDRType
        0x0,
        0x0,                         // recordChangeNumber
        PLDM_PDR_NUMERIC_SENSOR_PDR_MIN_LENGTH,
        0,                           // dataLength
        0,
        0,                           // PLDMTerminusHandle
        0x1,
        0x0,                         // sensorID=1
        PLDM_ENTITY_POWER_SUPPLY,
        0,                           // entityType=Power Supply(120)
        1,
        0,                           // entityInstanceNumber
        1,
        0,                           // containerID=1
        PLDM_NO_INIT,                // sensorInit
        false,                       // sensorAuxiliaryNamesPDR
        PLDM_SENSOR_UNIT_DEGRESS_C,  // baseUint(2)=degrees C
        0,                           // unitModifier = 0
        0,                           // rateUnit
        0,                           // baseOEMUnitHandle
        0,                           // auxUnit
        0,                           // auxUnitModifier
        0,                           // auxRateUnit
        0,                           // rel
        0,                           // auxOEMUnitHandle
        true,                        // isLinear
        PLDM_SENSOR_DATA_SIZE_UINT8, // sensorDataSize
        0,
        0,
        0x80,
        0x3f, // resolution=1.0
        0,
        0,
        0,
        0, // offset=0
        0,
        0, // accuracy
        0, // plusTolerance
        0, // minusTolerance
        2, // hysteresis = 2
        0, // supportedThresholds
        0, // thresholdAndHysteresisVolatility
        0,
        0,
        0x80,
        0x3f, // stateTransistionInterval=1.0
        0,
        0,
        0x80,
        0x3f,                          // updateInverval=1.0
        255,                           // maxReadable
        0,                             // minReadable
        PLDM_RANGE_FIELD_FORMAT_UINT8, // rangeFieldFormat
        0x18,                          // rangeFieldsupport
        0,                             // nominalValue
        0,                             // normalMax
        0,                             // normalMin
        WARNING_HIGH,                  // warningHigh
        20,                            // warningLow
        60,                            // criticalHigh
        10,                            // criticalLow
        0,                             // fatalHigh
        0                              // fatalLow
    };

    // add dummy numeric sensor
    termini[tid]->pdrs.emplace_back(pdr1);
    auto rc = termini[tid]->parsePDRs();
    EXPECT_EQ(true, rc);
    EXPECT_EQ(1, termini[tid]->numericSensors.size());

    uint8_t platformEventStatus = 0;

    EXPECT_CALL(eventManager, createSensorThresholdLogEntry(
                                  SensorThresholdWarningHighGoingHigh, _,
                                  SENSOR_READING, WARNING_HIGH))
        .Times(1)
        .WillRepeatedly(Return(PLDM_SUCCESS));
    std::vector<uint8_t> eventData{0x1,
                                   0x0, // sensor id
                                   PLDM_NUMERIC_SENSOR_STATE,
                                   PLDM_SENSOR_UPPERWARNING,
                                   PLDM_SENSOR_NORMAL,
                                   PLDM_SENSOR_DATA_SIZE_UINT8,
                                   SENSOR_READING};
    rc = eventManager.handlePlatformEvent(tid, PLDM_SENSOR_EVENT,
                                          eventData.data(), eventData.size());
    EXPECT_EQ(PLDM_SUCCESS, rc);
    EXPECT_EQ(PLDM_EVENT_NO_LOGGING, platformEventStatus);
}

TEST_F(EventManagerTest, getSensorThresholdMessageIdTest)
{
    std::string messageId{};
    messageId = eventManager.getSensorThresholdMessageId(PLDM_SENSOR_UNKNOWN,
                                                         PLDM_SENSOR_NORMAL);
    EXPECT_EQ(messageId, std::string{});

    messageId = eventManager.getSensorThresholdMessageId(
        PLDM_SENSOR_UNKNOWN, PLDM_SENSOR_LOWERWARNING);
    EXPECT_EQ(messageId, SensorThresholdWarningLowGoingLow);

    messageId = eventManager.getSensorThresholdMessageId(
        PLDM_SENSOR_UNKNOWN, PLDM_SENSOR_LOWERCRITICAL);
    EXPECT_EQ(messageId, SensorThresholdCriticalLowGoingLow);

    messageId = eventManager.getSensorThresholdMessageId(
        PLDM_SENSOR_UNKNOWN, PLDM_SENSOR_UPPERWARNING);
    EXPECT_EQ(messageId, SensorThresholdWarningHighGoingHigh);

    messageId = eventManager.getSensorThresholdMessageId(
        PLDM_SENSOR_UNKNOWN, PLDM_SENSOR_UPPERCRITICAL);
    EXPECT_EQ(messageId, SensorThresholdCriticalHighGoingHigh);

    messageId = eventManager.getSensorThresholdMessageId(
        PLDM_SENSOR_NORMAL, PLDM_SENSOR_LOWERWARNING);
    EXPECT_EQ(messageId, SensorThresholdWarningLowGoingLow);

    messageId = eventManager.getSensorThresholdMessageId(
        PLDM_SENSOR_LOWERWARNING, PLDM_SENSOR_LOWERCRITICAL);
    EXPECT_EQ(messageId, SensorThresholdCriticalLowGoingLow);

    messageId = eventManager.getSensorThresholdMessageId(
        PLDM_SENSOR_LOWERCRITICAL, PLDM_SENSOR_LOWERWARNING);
    EXPECT_EQ(messageId, SensorThresholdCriticalLowGoingHigh);

    messageId = eventManager.getSensorThresholdMessageId(
        PLDM_SENSOR_LOWERWARNING, PLDM_SENSOR_NORMAL);
    EXPECT_EQ(messageId, SensorThresholdWarningLowGoingHigh);

    messageId = eventManager.getSensorThresholdMessageId(
        PLDM_SENSOR_NORMAL, PLDM_SENSOR_UPPERWARNING);
    EXPECT_EQ(messageId, SensorThresholdWarningHighGoingHigh);

    messageId = eventManager.getSensorThresholdMessageId(
        PLDM_SENSOR_UPPERWARNING, PLDM_SENSOR_UPPERCRITICAL);
    EXPECT_EQ(messageId, SensorThresholdCriticalHighGoingHigh);

    messageId = eventManager.getSensorThresholdMessageId(
        PLDM_SENSOR_UPPERCRITICAL, PLDM_SENSOR_UPPERWARNING);
    EXPECT_EQ(messageId, SensorThresholdCriticalHighGoingLow);

    messageId = eventManager.getSensorThresholdMessageId(
        PLDM_SENSOR_UPPERWARNING, PLDM_SENSOR_NORMAL);
    EXPECT_EQ(messageId, SensorThresholdWarningHighGoingLow);
}

TEST_F(EventManagerTest, SetEventReceiverTest)
{
    // Add terminus
    auto mappedTid = terminusManager.mapTid(pldm::MctpInfo(1, "", "", 1));
    auto tid = mappedTid.value();
    termini[tid] =
        std::make_shared<Terminus>(tid, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);
    auto terminus = termini[tid];

    // queue eventMessageBufferSize response(bufferSize=32)
    const size_t eventMessageBufferSizeRespLen = 3;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + eventMessageBufferSizeRespLen>
        eventMessageBufferSizeResp{0x0, 0x02, 0x0d, PLDM_SUCCESS, 32, 0};
    auto rc = terminusManager.enqueueResponse(
        (pldm_msg*)eventMessageBufferSizeResp.data(),
        sizeof(eventMessageBufferSizeResp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    // queue eventMessageSupported response
    const size_t eventMessageSupportedLen = 7;
    PLDM_GET_PDR_REPOSITORY_INFO_RESP_BYTES;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + eventMessageSupportedLen>
        eventMessageSupportedResp{0x0,  0x02, 0x0c, PLDM_SUCCESS,
                                  0x0,  // synchronyConfiguration
                                  0x06, // synchronyConfigurationSupported
                                  3,    // numberEventClassReturned
                                  0x0,  0x5,  0xfa};
    rc = terminusManager.enqueueResponse(
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
    rc = terminusManager.enqueueResponse(
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
    rc = terminusManager.enqueueResponse((pldm_msg*)getPdrResp.data(),
                                         sizeof(getPdrResp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    // queue SetEventReceiver response
    const size_t SetEventReceiverLen = 1;
    PLDM_GET_PDR_REPOSITORY_INFO_RESP_BYTES;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + SetEventReceiverLen>
        SetEventReceiverResp{0x0, 0x02, 0x04, PLDM_SUCCESS};
    rc = terminusManager.enqueueResponse((pldm_msg*)SetEventReceiverResp.data(),
                                         sizeof(SetEventReceiverResp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    platformManager.initTerminus();
    EXPECT_EQ(true, terminus->initalized);
    EXPECT_EQ(1, terminus->pdrs.size());
    EXPECT_EQ(1, terminus->numericSensors.size());
    EXPECT_EQ(32, terminus->maxBufferSize);
    EXPECT_EQ(0x06, terminus->synchronyConfigurationSupported.byte);
}

TEST_F(EventManagerTest, pollForPlatformEventTaskMultipartTransferTest)
{
    // Add terminus
    auto mappedTid = terminusManager.mapTid(pldm::MctpInfo(1, "", "", 1));
    auto tid = mappedTid.value();
    termini[tid] =
        std::make_shared<Terminus>(tid, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);
    auto terminus = termini[tid];

    // queue pollForPlatformEventMessage first part response
    const size_t pollForPlatformEventMessage1Len = 22;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + pollForPlatformEventMessage1Len>
        pollForPlatformEventMessage1Resp{
            0x0,        0x02, 0x0d, PLDM_SUCCESS,
            tid,                         // TID
            0x1,        0x0,             // eventID
            0x1,        0x0,  0x0,  0x0, // nextDataTransferHandle
            PLDM_START,                  // transferFlag = start
            0xfa,                        // eventClass
            8,          0,    0,    0,   // eventDataSize
            0x01,                        // CPER event formatVersion= 0x01
            1,                        // formatType = single CPER section(0x01)
            10,         0,            // eventDataLength = 10
            1,          2,    3,    4 // eventData first part
        };
    auto rc = terminusManager.enqueueResponse(
        (pldm_msg*)pollForPlatformEventMessage1Resp.data(),
        sizeof(pollForPlatformEventMessage1Resp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    // queue pollForPlatformEventMessage last part response
    const size_t pollForPlatformEventMessage2Len = 24;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + pollForPlatformEventMessage2Len>
        pollForPlatformEventMessage2Resp{
            0x0,      0x02, 0x0d, PLDM_SUCCESS,
            tid,                       // TID
            0x1,      0x0,             // eventID
            0x2,      0x0,  0x0,  0x0, // nextDataTransferHandle
            PLDM_END,                  // transferFlag = end
            0xfa,                      // eventClass
            6,        0,    0,    0,   // eventDataSize
            5,        6,    7,    8,
            9,        0,               // eventData last part
            0x46,     0x7f, 0x6a, 0x5d // crc32
        };
    rc = terminusManager.enqueueResponse(
        (pldm_msg*)pollForPlatformEventMessage2Resp.data(),
        sizeof(pollForPlatformEventMessage2Resp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    // queue pollForPlatformEventMessage Ack response
    const size_t pollForPlatformEventMessage3Len = 4;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + pollForPlatformEventMessage3Len>
        pollForPlatformEventMessage3Resp{
            0x0, 0x02, 0x0d, PLDM_SUCCESS,
            tid,     // TID
            0x0, 0x0 // eventID
        };
    rc = terminusManager.enqueueResponse(
        (pldm_msg*)pollForPlatformEventMessage3Resp.data(),
        sizeof(pollForPlatformEventMessage3Resp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    EXPECT_CALL(eventManager, processCperEvent(_, _))
        .Times(1)
        .WillRepeatedly(Return(1));

    // start task to poll event from terminus
    eventManager.pollForPlatformEventTask(tid);
}
