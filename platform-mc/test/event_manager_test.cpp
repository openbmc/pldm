#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "mock_event_manager.hpp"
#include "mock_terminus_manager.hpp"
#include "platform-mc/platform_manager.hpp"
#include "platform-mc/terminus_manager.hpp"
#include "test/test_instance_id.hpp"
#include "utils_test.hpp"

#include <libpldm/base.h>
#include <libpldm/entity.h>
#include <libpldm/platform.h>

#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;

class EventManagerTest : public testing::Test
{
  protected:
    EventManagerTest() :
        bus(pldm::utils::DBusHandler::getBus()),
        event(sdeventplus::Event::get_default()), instanceIdDb(),
        reqHandler(pldmTransport, event, instanceIdDb, false,
                   std::chrono::seconds(1), 2, std::chrono::milliseconds(100)),
        terminusManager(event, reqHandler, instanceIdDb, termini, nullptr),
        eventManager(terminusManager, termini),
        platformManager(terminusManager, termini, nullptr)
    {}

    PldmTransport* pldmTransport = nullptr;
    sdbusplus::bus_t& bus;
    sdeventplus::Event event;
    TestInstanceIdDb instanceIdDb;
    pldm::requester::Handler<pldm::requester::Request> reqHandler;
    pldm::platform_mc::MockTerminusManager terminusManager;
    pldm::platform_mc::MockEventManager eventManager;
    pldm::platform_mc::PlatformManager platformManager;
    pldm::platform_mc::TerminiMapper termini{};
};

TEST_F(EventManagerTest, processNumericSensorEventTest)
{
#define SENSOR_READING 50
#define WARNING_HIGH 45
    pldm_tid_t tid = 1;
    termini[tid] = std::make_shared<pldm::platform_mc::Terminus>(
        tid, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
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
        0,    // offset=0
        0,
        0,    // accuracy
        0,    // plusTolerance
        0,    // minusTolerance
        2,    // hysteresis = 2
        0x1b, // supportedThresholds
        0,    // thresholdAndHysteresisVolatility
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

    std::vector<uint8_t> pdr2{
        0x1, 0x0, 0x0,
        0x0,                             // record handle
        0x1,                             // PDRHeaderVersion
        PLDM_ENTITY_AUXILIARY_NAMES_PDR, // PDRType
        0x1,
        0x0,                             // recordChangeNumber
        0x11,
        0,                               // dataLength
        /* Entity Auxiliary Names PDR Data*/
        3,
        0x80, // entityType system software
        0x1,
        0x0,  // Entity instance number =1
        0,
        0,    // Overal system
        0,    // shared Name Count one name only
        01,   // nameStringCount
        0x65, 0x6e, 0x00,
        0x00, // Language Tag "en"
        0x53, 0x00, 0x30, 0x00,
        0x00  // Entity Name "S0"
    };

    // add dummy numeric sensor
    termini[tid]->pdrs.emplace_back(pdr1);
    termini[tid]->pdrs.emplace_back(pdr2);
    termini[tid]->parseTerminusPDRs();
    // Run event loop for a few seconds to let sensor creation
    // defer tasks be run. May increase time when sensor num is large
    utils::runEventLoopForSeconds(event, 1);
    EXPECT_EQ(1, termini[tid]->numericSensors.size());

    uint8_t platformEventStatus = 0;

    std::vector<uint8_t> eventData{
        0x1,
        0x0, // sensor id
        PLDM_NUMERIC_SENSOR_STATE,
        PLDM_SENSOR_UPPERWARNING,
        PLDM_SENSOR_NORMAL,
        PLDM_SENSOR_DATA_SIZE_UINT8,
        SENSOR_READING};
    auto rc = eventManager.handlePlatformEvent(
        tid, 0x00, PLDM_SENSOR_EVENT, eventData.data(), eventData.size());
    EXPECT_EQ(PLDM_SUCCESS, rc);
    EXPECT_EQ(PLDM_EVENT_NO_LOGGING, platformEventStatus);
}

TEST_F(EventManagerTest, SetEventReceiverTest)
{
    // Add terminus
    auto mappedTid =
        terminusManager.mapTid(pldm::MctpInfo(10, "", "", 1, std::nullopt));
    auto tid = mappedTid.value();
    termini[tid] = std::make_shared<pldm::platform_mc::Terminus>(
        tid, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
    auto terminus = termini[tid];

    /* Set supported command by terminus */
    auto size = PLDM_MAX_TYPES * (PLDM_MAX_CMDS_PER_TYPE / 8);
    std::vector<uint8_t> pldmCmds(size);
    uint8_t type = PLDM_PLATFORM;
    uint8_t cmd = PLDM_GET_PDR_REPOSITORY_INFO;
    auto idx = type * (PLDM_MAX_CMDS_PER_TYPE / 8) + (cmd / 8);
    pldmCmds[idx] = pldmCmds[idx] | (1 << (cmd % 8));
    cmd = PLDM_GET_PDR;
    idx = type * (PLDM_MAX_CMDS_PER_TYPE / 8) + (cmd / 8);
    pldmCmds[idx] = pldmCmds[idx] | (1 << (cmd % 8));
    cmd = PLDM_EVENT_MESSAGE_SUPPORTED;
    idx = type * (PLDM_MAX_CMDS_PER_TYPE / 8) + (cmd / 8);
    pldmCmds[idx] = pldmCmds[idx] | (1 << (cmd % 8));
    cmd = PLDM_EVENT_MESSAGE_BUFFER_SIZE;
    idx = type * (PLDM_MAX_CMDS_PER_TYPE / 8) + (cmd / 8);
    pldmCmds[idx] = pldmCmds[idx] | (1 << (cmd % 8));
    cmd = PLDM_SET_EVENT_RECEIVER;
    idx = type * (PLDM_MAX_CMDS_PER_TYPE / 8) + (cmd / 8);
    pldmCmds[idx] = pldmCmds[idx] | (1 << (cmd % 8));
    termini[tid]->setSupportedCommands(pldmCmds);

    EXPECT_EQ(true, termini[tid]->doesSupportCommand(PLDM_PLATFORM,
                                                     PLDM_SET_EVENT_RECEIVER));
    EXPECT_EQ(true, termini[tid]->doesSupportCommand(
                        PLDM_PLATFORM, PLDM_EVENT_MESSAGE_BUFFER_SIZE));
    EXPECT_EQ(true, termini[tid]->doesSupportCommand(
                        PLDM_PLATFORM, PLDM_EVENT_MESSAGE_SUPPORTED));
    EXPECT_EQ(true,
              termini[tid]->doesSupportCommand(PLDM_PLATFORM, PLDM_GET_PDR));
    EXPECT_EQ(true, termini[tid]->doesSupportCommand(
                        PLDM_PLATFORM, PLDM_GET_PDR_REPOSITORY_INFO));

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
            2,   0x0,  0x0,  0x0,                    // recordCount
            0x0, 0x1,  0x0,  0x0,                    // repositorySize
            59,  0x0,  0x0,  0x0,                    // largestRecordSize
            0x0 // dataTransferHandleTimeout
        };
    auto rc = terminusManager.enqueueResponse(
        new (getPDRRepositoryInfoResp.data()) pldm_msg,
        sizeof(getPDRRepositoryInfoResp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    // queue getPDR responses
    const size_t getPdrRespLen = 81;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + getPdrRespLen> getPdrResp{
        0x0, 0x02, 0x51, PLDM_SUCCESS, 0x1, 0x0, 0x0, 0x0, // nextRecordHandle
        0x0, 0x0, 0x0, 0x0, // nextDataTransferHandle
        0x5,                // transferFlag
        69, 0x0,            // responseCount
        // numeric Sensor PDR
        0x0, 0x0, 0x0,
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
    rc = terminusManager.enqueueResponse(new (getPdrResp.data()) pldm_msg,
                                         sizeof(getPdrResp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    const size_t getPdrAuxNameRespLen = 39;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + getPdrAuxNameRespLen>
        getPdrAuxNameResp{
            0x0, 0x02, 0x51, PLDM_SUCCESS, 0x0, 0x0, 0x0,
            0x0,                // nextRecordHandle
            0x0, 0x0, 0x0, 0x0, // nextDataTransferHandle
            0x5,                // transferFlag
            0x1b, 0x0,          // responseCount
            // Common PDR Header
            0x1, 0x0, 0x0,
            0x0,                             // record handle
            0x1,                             // PDRHeaderVersion
            PLDM_ENTITY_AUXILIARY_NAMES_PDR, // PDRType
            0x1,
            0x0,                             // recordChangeNumber
            0x11,
            0,                               // dataLength
            /* Entity Auxiliary Names PDR Data*/
            3,
            0x80, // entityType system software
            0x1,
            0x0,  // Entity instance number =1
            0,
            0,    // Overal system
            0,    // shared Name Count one name only
            01,   // nameStringCount
            0x65, 0x6e, 0x00,
            0x00, // Language Tag "en"
            0x53, 0x00, 0x30, 0x00,
            0x00  // Entity Name "S0"
        };
    rc = terminusManager.enqueueResponse(
        new (getPdrAuxNameResp.data()) pldm_msg, sizeof(getPdrAuxNameResp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    // queue eventMessageBufferSize response(bufferSize=32)
    const size_t eventMessageBufferSizeRespLen = 3;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + eventMessageBufferSizeRespLen>
        eventMessageBufferSizeResp{0x0, 0x02, 0x0d, PLDM_SUCCESS, 32, 0};
    rc = terminusManager.enqueueResponse(
        new (eventMessageBufferSizeResp.data()) pldm_msg,
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
        new (eventMessageSupportedResp.data()) pldm_msg,
        sizeof(eventMessageSupportedResp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    terminusManager.updateMctpEndpointAvailability(
        pldm::MctpInfo(10, "", "", 1, std::nullopt), true);

    // queue SetEventReceiver response
    const size_t SetEventReceiverLen = 1;
    PLDM_GET_PDR_REPOSITORY_INFO_RESP_BYTES;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + SetEventReceiverLen>
        SetEventReceiverResp{0x0, 0x02, 0x04, PLDM_SUCCESS};
    rc = terminusManager.enqueueResponse(
        new (SetEventReceiverResp.data()) pldm_msg,
        sizeof(SetEventReceiverResp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    // should finish immediately
    stdexec::sync_wait(platformManager.initTerminus());
    // Run event loop for a few seconds to let sensor creation
    // defer tasks be run. May increase time when sensor num is large
    utils::runEventLoopForSeconds(event, 1);
    EXPECT_EQ(true, terminus->initialized);
    EXPECT_EQ(32, terminus->maxBufferSize);
    EXPECT_EQ(0x06, terminus->synchronyConfigurationSupported.byte);
    EXPECT_EQ(2, terminus->pdrs.size());
    EXPECT_EQ(1, terminus->numericSensors.size());
}

TEST_F(EventManagerTest, updateAvailableState)
{
    pldm_tid_t tid = 1;
    eventManager.updateAvailableState(tid, true);
    EXPECT_EQ(true, eventManager.getAvailableState(tid));
    eventManager.updateAvailableState(tid, false);
    EXPECT_EQ(false, eventManager.getAvailableState(tid));
    eventManager.updateAvailableState(2, false);
    EXPECT_EQ(false, eventManager.getAvailableState(tid));
}

TEST_F(EventManagerTest, pollForPlatformEventTaskMultipartTransferTest)
{
    // Add terminus
    auto mappedTid =
        terminusManager.mapTid(pldm::MctpInfo(10, "", "", 1, std::nullopt));
    auto tid = mappedTid.value();
    termini[tid] = std::make_shared<pldm::platform_mc::Terminus>(
        tid, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
    auto terminus = termini[tid];

    // queue pollForPlatformEventMessage first part response
    const size_t pollForPlatformEventMessage1Len = 22;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + pollForPlatformEventMessage1Len>
        pollForPlatformEventMessage1Resp{
            0x0,
            0x02,
            0x0d,
            PLDM_SUCCESS,
            tid, // TID
            0x1,
            0x0, // eventID
            0x1,
            0x0,
            0x0,
            0x0,                          // nextDataTransferHandle
            PLDM_PLATFORM_TRANSFER_START, // transferFlag = start
            PLDM_CPER_EVENT,              // eventClass
            8,
            0,
            0,
            0,    // eventDataSize
            0x01, // CPER event formatVersion= 0x01
            1,    // formatType = single CPER section(0x01)
            10,
            0,    // eventDataLength = 10
            1,
            2,
            3,
            4 // eventData first part
        };
    auto rc = terminusManager.enqueueResponse(
        new (pollForPlatformEventMessage1Resp.data()) pldm_msg,
        sizeof(pollForPlatformEventMessage1Resp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    // queue pollForPlatformEventMessage last part response
    const size_t pollForPlatformEventMessage2Len = 24;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + pollForPlatformEventMessage2Len>
        pollForPlatformEventMessage2Resp{
            0x0,
            0x02,
            0x0d,
            PLDM_SUCCESS,
            tid, // TID
            0x1,
            0x0, // eventID
            0x2,
            0x0,
            0x0,
            0x0,                        // nextDataTransferHandle
            PLDM_PLATFORM_TRANSFER_END, // transferFlag = end
            PLDM_CPER_EVENT,            // eventClass
            6,
            0,
            0,
            0, // eventDataSize
            5,
            6,
            7,
            8,
            9,
            0, // eventData last part
            0x46,
            0x7f,
            0x6a,
            0x5d // crc32
        };
    rc = terminusManager.enqueueResponse(
        new (pollForPlatformEventMessage2Resp.data()) pldm_msg,
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
        new (pollForPlatformEventMessage3Resp.data()) pldm_msg,
        sizeof(pollForPlatformEventMessage3Resp));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    terminusManager.updateMctpEndpointAvailability(
        pldm::MctpInfo(10, "", "", 1, std::nullopt), true);

    EXPECT_CALL(eventManager, processCperEvent(_, _, _, _))
        .Times(1)
        .WillRepeatedly(Return(1));

    // start task to poll event from terminus
    // should finish immediately
    stdexec::sync_wait(eventManager.pollForPlatformEventTask(tid, 0x0000));
}

TEST(EventManagerNumericEventTest, HandleNormalToUpperWarning)
{
    using namespace pldm::platform_mc;
    unsigned int callCount = 0;
    auto handle =
        [&callCount](pldm::utils::Level level, pldm::utils::Direction direction,
                     double rawValue, bool newAlarm, bool assert) {
            callCount++;
            EXPECT_EQ(rawValue, 10.0);
            EXPECT_TRUE(newAlarm);
            EXPECT_TRUE(assert);
            EXPECT_EQ(level, pldm::utils::Level::WARNING);
            EXPECT_EQ(direction, pldm::utils::Direction::HIGH);
            EXPECT_EQ(callCount, 1);
            return PLDM_SUCCESS;
        };
    int rc = triggerNumericSensorThresholdEvent(handle, PLDM_SENSOR_NORMAL,
                                                PLDM_SENSOR_UPPERWARNING, 10.0);
    ASSERT_EQ(rc, PLDM_SUCCESS);
}

TEST(EventManagerNumericEventTest, HandleUpperWarningToNormal)
{
    using namespace pldm::platform_mc;
    unsigned int callCount = 0;
    auto handle =
        [&callCount](pldm::utils::Level level, pldm::utils::Direction direction,
                     double rawValue, bool newAlarm, bool assert) {
            callCount++;
            EXPECT_EQ(rawValue, 4.0);
            EXPECT_FALSE(newAlarm);
            EXPECT_FALSE(assert);
            EXPECT_EQ(level, pldm::utils::Level::WARNING);
            EXPECT_EQ(direction, pldm::utils::Direction::HIGH);
            EXPECT_EQ(callCount, 1);
            return PLDM_SUCCESS;
        };
    int rc = triggerNumericSensorThresholdEvent(
        handle, PLDM_SENSOR_UPPERWARNING, PLDM_SENSOR_NORMAL, 4.0);
    ASSERT_EQ(rc, PLDM_SUCCESS);
}

TEST(EventManagerNumericEventTest, HandleNormalToUpperCritical)
{
    using namespace pldm::platform_mc;
    unsigned int callCount = 0;
    auto handle =
        [&callCount](pldm::utils::Level level, pldm::utils::Direction direction,
                     double rawValue, bool newAlarm, bool assert) {
            callCount++;
            EXPECT_EQ(rawValue, 10.0);
            EXPECT_TRUE(newAlarm);
            EXPECT_TRUE(assert);
            if (callCount == 1)
            {
                EXPECT_EQ(level, pldm::utils::Level::WARNING);
            }
            else
            {
                EXPECT_EQ(level, pldm::utils::Level::CRITICAL);
            }
            EXPECT_EQ(direction, pldm::utils::Direction::HIGH);
            EXPECT_LE(callCount, 2);
            return PLDM_SUCCESS;
        };
    int rc = triggerNumericSensorThresholdEvent(
        handle, PLDM_SENSOR_NORMAL, PLDM_SENSOR_UPPERCRITICAL, 10.0);
    ASSERT_EQ(rc, PLDM_SUCCESS);
}

TEST(EventManagerNumericEventTest, HandleUpperCriticalToNormal)
{
    using namespace pldm::platform_mc;
    unsigned int callCount = 0;
    auto handle =
        [&callCount](pldm::utils::Level level, pldm::utils::Direction direction,
                     double rawValue, bool newAlarm, bool assert) {
            callCount++;
            EXPECT_EQ(rawValue, 10.0);
            if (callCount == 1)
            {
                // First deasserts critical
                EXPECT_EQ(level, pldm::utils::Level::CRITICAL);
                EXPECT_FALSE(newAlarm);
                EXPECT_FALSE(assert);
            }
            else if (callCount == 2)
            {
                // second  asserts warning just before deasserting it.
                EXPECT_TRUE(newAlarm);
                EXPECT_TRUE(assert);
                EXPECT_EQ(level, pldm::utils::Level::WARNING);
            }
            else
            {
                // Third and final time we deassert warning.
                EXPECT_FALSE(newAlarm);
                EXPECT_FALSE(assert);
                EXPECT_EQ(level, pldm::utils::Level::WARNING);
            }
            EXPECT_EQ(direction, pldm::utils::Direction::HIGH);
            EXPECT_LE(callCount, 3);
            return PLDM_SUCCESS;
        };
    int rc = triggerNumericSensorThresholdEvent(
        handle, PLDM_SENSOR_UPPERCRITICAL, PLDM_SENSOR_NORMAL, 10.0);
    ASSERT_EQ(rc, PLDM_SUCCESS);
}

TEST(EventManagerNumericEventTest, HandleNormalToLowerWarning)
{
    using namespace pldm::platform_mc;
    unsigned int callCount = 0;
    auto handle =
        [&callCount](pldm::utils::Level level, pldm::utils::Direction direction,
                     double rawValue, bool newAlarm, bool assert) {
            callCount++;
            EXPECT_EQ(rawValue, 10.0);
            EXPECT_TRUE(newAlarm);
            EXPECT_TRUE(assert);
            EXPECT_EQ(level, pldm::utils::Level::WARNING);
            EXPECT_EQ(direction, pldm::utils::Direction::LOW);
            EXPECT_EQ(callCount, 1);
            return PLDM_SUCCESS;
        };
    int rc = triggerNumericSensorThresholdEvent(handle, PLDM_SENSOR_NORMAL,
                                                PLDM_SENSOR_LOWERWARNING, 10.0);
    ASSERT_EQ(rc, PLDM_SUCCESS);
}

TEST(EventManagerNumericEventTest, HandleLowerWarningToNormal)
{
    using namespace pldm::platform_mc;
    unsigned int callCount = 0;
    auto handle =
        [&callCount](pldm::utils::Level level, pldm::utils::Direction direction,
                     double rawValue, bool newAlarm, bool assert) {
            callCount++;
            EXPECT_EQ(rawValue, 4.0);
            EXPECT_FALSE(newAlarm);
            EXPECT_FALSE(assert);
            EXPECT_EQ(level, pldm::utils::Level::WARNING);
            EXPECT_EQ(direction, pldm::utils::Direction::LOW);
            EXPECT_EQ(callCount, 1);
            return PLDM_SUCCESS;
        };
    int rc = triggerNumericSensorThresholdEvent(
        handle, PLDM_SENSOR_LOWERWARNING, PLDM_SENSOR_NORMAL, 4.0);
    ASSERT_EQ(rc, PLDM_SUCCESS);
}

TEST(EventManagerNumericEventTest, HandleNormalToLowerCritical)
{
    using namespace pldm::platform_mc;
    unsigned int callCount = 0;
    auto handle =
        [&callCount](pldm::utils::Level level, pldm::utils::Direction direction,
                     double rawValue, bool newAlarm, bool assert) {
            callCount++;
            EXPECT_EQ(rawValue, 10.0);
            EXPECT_TRUE(newAlarm);
            EXPECT_TRUE(assert);
            if (callCount == 1)
            {
                EXPECT_EQ(level, pldm::utils::Level::WARNING);
            }
            else
            {
                EXPECT_EQ(level, pldm::utils::Level::CRITICAL);
            }
            EXPECT_EQ(direction, pldm::utils::Direction::LOW);
            EXPECT_LE(callCount, 2);
            return PLDM_SUCCESS;
        };
    int rc = triggerNumericSensorThresholdEvent(
        handle, PLDM_SENSOR_NORMAL, PLDM_SENSOR_LOWERCRITICAL, 10.0);
    ASSERT_EQ(rc, PLDM_SUCCESS);
}

TEST(EventManagerNumericEventTest, HandleLowerCriticalToNormal)
{
    using namespace pldm::platform_mc;
    unsigned int callCount = 0;
    auto handle =
        [&callCount](pldm::utils::Level level, pldm::utils::Direction direction,
                     double rawValue, bool newAlarm, bool assert) {
            callCount++;
            EXPECT_EQ(rawValue, 10.0);
            if (callCount == 1)
            {
                // First deasserts critical
                EXPECT_EQ(level, pldm::utils::Level::CRITICAL);
                EXPECT_FALSE(newAlarm);
                EXPECT_FALSE(assert);
            }
            else if (callCount == 2)
            {
                // second  asserts warning just before deasserting it.
                EXPECT_TRUE(newAlarm);
                EXPECT_TRUE(assert);
                EXPECT_EQ(level, pldm::utils::Level::WARNING);
            }
            else
            {
                // Third and final time we deassert warning.
                EXPECT_FALSE(newAlarm);
                EXPECT_FALSE(assert);
                EXPECT_EQ(level, pldm::utils::Level::WARNING);
            }
            EXPECT_EQ(direction, pldm::utils::Direction::LOW);
            EXPECT_LE(callCount, 3);
            return PLDM_SUCCESS;
        };
    int rc = triggerNumericSensorThresholdEvent(
        handle, PLDM_SENSOR_LOWERCRITICAL, PLDM_SENSOR_NORMAL, 10.0);
    ASSERT_EQ(rc, PLDM_SUCCESS);
}

TEST(EventManagerNumericEventTest, HandleLowerCriticalToUpperCritical)
{
    using namespace pldm::platform_mc;
    unsigned int callCount = 0;
    auto handle =
        [&callCount](pldm::utils::Level level, pldm::utils::Direction direction,
                     double rawValue, bool newAlarm, bool assert) {
            callCount++;
            EXPECT_EQ(rawValue, 10.0);
            if (callCount == 1)
            {
                // First deasserts critical
                EXPECT_EQ(level, pldm::utils::Level::CRITICAL);
                EXPECT_FALSE(newAlarm);
                EXPECT_FALSE(assert);
                EXPECT_EQ(direction, pldm::utils::Direction::LOW);
            }
            else if (callCount == 2)
            {
                // second  asserts low-warning just before deasserting it.
                EXPECT_TRUE(newAlarm);
                EXPECT_TRUE(assert);
                EXPECT_EQ(level, pldm::utils::Level::WARNING);
                EXPECT_EQ(direction, pldm::utils::Direction::LOW);
            }
            else if (callCount == 3)
            {
                // third deasserts low-warning
                EXPECT_FALSE(newAlarm);
                EXPECT_FALSE(assert);
                EXPECT_EQ(level, pldm::utils::Level::WARNING);
                EXPECT_EQ(direction, pldm::utils::Direction::LOW);
            }
            else if (callCount == 4)
            {
                // forth asserts upper warning
                EXPECT_TRUE(newAlarm);
                EXPECT_TRUE(assert);
                EXPECT_EQ(level, pldm::utils::Level::WARNING);
                EXPECT_EQ(direction, pldm::utils::Direction::HIGH);
            }
            else
            {
                // fifth, asserts upper critical
                EXPECT_TRUE(newAlarm);
                EXPECT_TRUE(assert);
                EXPECT_EQ(level, pldm::utils::Level::CRITICAL);
                EXPECT_EQ(direction, pldm::utils::Direction::HIGH);
            }
            EXPECT_LE(callCount, 5);
            return PLDM_SUCCESS;
        };
    int rc = triggerNumericSensorThresholdEvent(
        handle, PLDM_SENSOR_LOWERCRITICAL, PLDM_SENSOR_UPPERCRITICAL, 10.0);
    ASSERT_EQ(callCount, 5);
    ASSERT_EQ(rc, PLDM_SUCCESS);
}
