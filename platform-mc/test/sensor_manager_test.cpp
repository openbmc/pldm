#include "common/start_lifetime_as.hpp"
#include "common/types.hpp"
#include "mock_sensor_manager.hpp"
#include "mock_terminus_manager.hpp"
#include "platform-mc/sensor_manager.hpp"
#include "platform-mc/terminus_manager.hpp"
#include "test/test_instance_id.hpp"
#include "utils_test.hpp"

#include <sdeventplus/event.hpp>

#include <optional>
#include <tuple>

#include <gtest/gtest.h>

using namespace ::testing;

class SensorManagerTest : public testing::Test
{
  protected:
    SensorManagerTest() :
        bus(pldm::utils::DBusHandler::getBus()),
        event(sdeventplus::Event::get_default()), instanceIdDb(),
        reqHandler(pldmTransport, event, instanceIdDb, false),
        terminusManager(event, reqHandler, instanceIdDb, termini, nullptr,
                        pldm::BmcMctpEid),
        sensorManager(event, terminusManager, termini, nullptr)
    {}

    PldmTransport* pldmTransport = nullptr;
    sdbusplus::bus_t& bus;
    sdeventplus::Event event;
    TestInstanceIdDb instanceIdDb;
    pldm::requester::Handler<pldm::requester::Request> reqHandler;
    pldm::platform_mc::TerminusManager terminusManager;
    pldm::platform_mc::MockSensorManager sensorManager;
    std::map<pldm_tid_t, std::shared_ptr<pldm::platform_mc::Terminus>> termini;

    std::vector<uint8_t> pdr1{
        0x1,
        0x0,
        0x0,
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
        PLDM_ENTITY_POWER_SUPPLY,
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
        PLDM_RANGE_FIELD_FORMAT_SINT8, // sensorDataSize
        0,
        0,
        0xc0,
        0x3f, // resolution=1.5
        0,
        0,
        0x80,
        0x3f, // offset=1.0
        0,
        0,    // accuracy
        0,    // plusTolerance
        0,    // minusTolerance
        2,    // hysteresis
        0,    // supportedThresholds
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
        0,    // Overall system
        0,    // shared Name Count one name only
        01,   // nameStringCount
        0x65, 0x6e, 0x00,
        0x00, // Language Tag "en"
        0x53, 0x00, 0x30, 0x00,
        0x00  // Entity Name "S0"
    };
};

TEST_F(SensorManagerTest, sensorPollingTest)
{
    uint64_t seconds = 10;
    pldm_tid_t tid = 1;
    termini[tid] = std::make_shared<pldm::platform_mc::Terminus>(tid, 0, event);
    termini[tid]->pdrs.push_back(pdr1);
    termini[tid]->pdrs.push_back(pdr2);
    termini[tid]->parseTerminusPDRs();

    uint64_t t0 = 0, t1 = 0;
    ASSERT_TRUE(sd_event_now(event.get(), CLOCK_MONOTONIC, &t0) >= 0);
    ON_CALL(sensorManager, doSensorPolling(tid))
        .WillByDefault([this, &t0, &t1](unsigned char) {
            ASSERT_TRUE(sd_event_now(event.get(), CLOCK_MONOTONIC, &t1) >= 0);
            EXPECT_GE(t1 - t0, pldm::platform_mc::SENSOR_POLLING_TIME * 1000);
            t0 = t1;
        });
    EXPECT_CALL(sensorManager, doSensorPolling(tid))
        .Times(AtLeast(2))
        .WillRepeatedly(Return());

    sensorManager.startPolling(tid);

    utils::runEventLoopForSeconds(event, seconds);

    sensorManager.stopPolling(tid);
}

class StateSensorManagerTest : public testing::Test
{
  protected:
    StateSensorManagerTest() :
        bus(pldm::utils::DBusHandler::getBus()),
        event(sdeventplus::Event::get_default()), instanceIdDb(),
        reqHandler(pldmTransport, event, instanceIdDb, false),
        mockTerminusManager(event, reqHandler, instanceIdDb, termini, nullptr),
        sensorManager(event, mockTerminusManager, termini, nullptr)
    {}

    /** @brief Discover a terminus whose only State Sensor PDR describes a
     *         composite of two component sensors on a power supply entity,
     *         and start its polling so the state sensor commands can be sent.
     */
    std::shared_ptr<pldm::platform_mc::StateSensor> discoverStateSensor()
    {
        auto mappedTid = mockTerminusManager.mapTid(
            pldm::MctpInfo(10, "", "", 1, std::nullopt));
        tid = mappedTid.value();
        termini[tid] = std::make_shared<pldm::platform_mc::Terminus>(
            tid, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
        termini[tid]->pdrs.push_back(stateSensorPdr);
        termini[tid]->pdrs.push_back(entityAuxNamesPdr);

        /* SetStateSensorEnables is only sent to a terminus which supports it */
        std::vector<uint8_t> pldmCmds(
            PLDM_MAX_TYPES * (PLDM_MAX_CMDS_PER_TYPE / 8));
        auto idx = PLDM_PLATFORM * (PLDM_MAX_CMDS_PER_TYPE / 8) +
                   (PLDM_SET_STATE_SENSOR_ENABLES / 8);
        pldmCmds[idx] = pldmCmds[idx] |
                        (1 << (PLDM_SET_STATE_SENSOR_ENABLES % 8));
        termini[tid]->setSupportedCommands(pldmCmds);

        termini[tid]->parseTerminusPDRs();
        sensorManager.startPolling(tid);

        if (termini[tid]->stateSensors.empty())
        {
            return nullptr;
        }
        return termini[tid]->stateSensors[0];
    }

    PldmTransport* pldmTransport = nullptr;
    sdbusplus::bus_t& bus;
    sdeventplus::Event event;
    TestInstanceIdDb instanceIdDb;
    pldm::requester::Handler<pldm::requester::Request> reqHandler;
    pldm::platform_mc::MockTerminusManager mockTerminusManager;
    pldm::platform_mc::MockSensorManager sensorManager;
    std::map<pldm_tid_t, std::shared_ptr<pldm::platform_mc::Terminus>> termini;
    pldm_tid_t tid = 0;

    // State Sensor PDR: sensorID = 1, composite of two component sensors
    std::vector<uint8_t> stateSensorPdr{
        0x1, 0x0, 0x0,
        0x0,                   // record handle
        0x1,                   // PDRHeaderVersion
        PLDM_STATE_SENSOR_PDR, // PDRType
        0x0,
        0x0,                   // recordChangeNumber
        21,
        0,                     // dataLength
        /* State Sensor PDR Data*/
        0,
        0,            // PLDMTerminusHandle
        0x1,
        0x0,          // sensorID = 1
        PLDM_ENTITY_POWER_SUPPLY,
        0,            // entityType power supply
        1,
        0,            // entityInstanceNumber = 1
        0x1,
        0x0,          // containerID = 1
        PLDM_NO_INIT, // sensorInit
        false,        // sensorAuxiliaryNamesPDR
        2,            // compositeSensorCount
        0x1,
        0x0,          // stateSetID[0] = 1
        0x1,          // possibleStatesSize[0]
        0x6,          // possibleStates[0] = {1,2}
        0x3,
        0x0,          // stateSetID[1] = 3
        0x1,          // possibleStatesSize[1]
        0x1e          // possibleStates[1] = {1,2,3,4}
    };

    // Entity Auxiliary Names PDR: terminus name "S0"
    std::vector<uint8_t> entityAuxNamesPdr{
        0x2, 0x0, 0x0,
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
        0x0,  // Entity instance number = 1
        0,
        0,    // Overall system
        0,    // shared Name Count one name only
        01,   // nameStringCount
        0x65, 0x6e, 0x00,
        0x00, // Language Tag "en"
        0x53, 0x00, 0x30, 0x00,
        0x00  // Entity Name "S0"
    };
};

TEST_F(StateSensorManagerTest, setStateSensorEnablesTest)
{
    auto sensor = discoverStateSensor();
    ASSERT_NE(nullptr, sensor);
    EXPECT_EQ(false, sensor->enabled);
    EXPECT_EQ(false, sensor->enableRejected);

    std::array<uint8_t, sizeof(pldm_msg_hdr) +
                            PLDM_PLATFORM_SET_STATE_SENSOR_ENABLES_RESP_BYTES>
        resp{0x0, 0x02, PLDM_SET_STATE_SENSOR_ENABLES, PLDM_SUCCESS};
    EXPECT_EQ(PLDM_SUCCESS,
              mockTerminusManager.enqueueResponse(
                  std::start_lifetime_as<pldm_msg>(resp.data()), sizeof(resp)));

    auto res = stdexec::sync_wait(sensorManager.setStateSensorEnables(sensor));
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(PLDM_SUCCESS, std::get<0>(*res));

    /* The request enables every component sensor of the composite and asks
     * for no event message, and is trimmed to the two encoded fields
     */
    const auto& request = mockTerminusManager.lastRequest;
    ASSERT_EQ(sizeof(pldm_msg_hdr) + 3 + (2 * 2), request.size());
    EXPECT_EQ(1, request[sizeof(pldm_msg_hdr)]);     // sensorID low byte
    EXPECT_EQ(0, request[sizeof(pldm_msg_hdr) + 1]); // sensorID high byte
    EXPECT_EQ(2, request[sizeof(pldm_msg_hdr) + 2]); // compositeSensorCount
    for (size_t offset = 0; offset < 2; offset++)
    {
        EXPECT_EQ(PLDM_SENSOR_ENABLED,
                  request[sizeof(pldm_msg_hdr) + 3 + (offset * 2)]);
        EXPECT_EQ(PLDM_EVENTS_DISABLED,
                  request[sizeof(pldm_msg_hdr) + 4 + (offset * 2)]);
    }
}

TEST_F(StateSensorManagerTest, setStateSensorEnablesErrorTest)
{
    auto sensor = discoverStateSensor();
    ASSERT_NE(nullptr, sensor);

    std::array<uint8_t, sizeof(pldm_msg_hdr) +
                            PLDM_PLATFORM_SET_STATE_SENSOR_ENABLES_RESP_BYTES>
        resp{0x0, 0x02, PLDM_SET_STATE_SENSOR_ENABLES,
             PLDM_PLATFORM_INVALID_SENSOR_ID};
    EXPECT_EQ(PLDM_SUCCESS,
              mockTerminusManager.enqueueResponse(
                  std::start_lifetime_as<pldm_msg>(resp.data()), sizeof(resp)));

    /* The completion code of the terminus is returned, and the answer is
     * definitive, so the command is not sent for the sensor again
     */
    auto res = stdexec::sync_wait(sensorManager.setStateSensorEnables(sensor));
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(PLDM_PLATFORM_INVALID_SENSOR_ID, std::get<0>(*res));
    EXPECT_EQ(false, sensor->enabled);
    EXPECT_EQ(true, sensor->enableRejected);
}

TEST_F(StateSensorManagerTest, getStateSensorReadingsTest)
{
    auto sensor = discoverStateSensor();
    ASSERT_NE(nullptr, sensor);

    std::array<uint8_t, sizeof(pldm_msg_hdr) + 2 + (4 * 2)> resp{
        0x0,
        0x02,
        PLDM_GET_STATE_SENSOR_READINGS,
        PLDM_SUCCESS,
        2,                    // compositeSensorCount
        PLDM_SENSOR_ENABLED,  // sensorOpState[0]
        0x1,                  // presentState[0]
        0x1,                  // previousState[0]
        0x1,                  // eventState[0]
        PLDM_SENSOR_DISABLED, // sensorOpState[1]
        0x2,                  // presentState[1]
        0x2,                  // previousState[1]
        0x2                   // eventState[1]
    };
    EXPECT_EQ(PLDM_SUCCESS,
              mockTerminusManager.enqueueResponse(
                  std::start_lifetime_as<pldm_msg>(resp.data()), sizeof(resp)));

    auto res = stdexec::sync_wait(sensorManager.getStateSensorReadings(sensor));
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(PLDM_SUCCESS, std::get<0>(*res));

    /* The request reads the sensor without rearming any component sensor */
    const auto& request = mockTerminusManager.lastRequest;
    ASSERT_EQ(sizeof(pldm_msg_hdr) + PLDM_GET_STATE_SENSOR_READINGS_REQ_BYTES,
              request.size());
    EXPECT_EQ(1, request[sizeof(pldm_msg_hdr)]);     // sensorID low byte
    EXPECT_EQ(0, request[sizeof(pldm_msg_hdr) + 1]); // sensorID high byte
    EXPECT_EQ(0, request[sizeof(pldm_msg_hdr) + 2]); // sensorRearm
}

TEST_F(StateSensorManagerTest, getStateSensorReadingsErrorTest)
{
    auto sensor = discoverStateSensor();
    ASSERT_NE(nullptr, sensor);

    std::array<uint8_t, sizeof(pldm_msg_hdr) + 1> resp{
        0x0, 0x02, PLDM_GET_STATE_SENSOR_READINGS,
        PLDM_PLATFORM_INVALID_SENSOR_ID};
    EXPECT_EQ(PLDM_SUCCESS,
              mockTerminusManager.enqueueResponse(
                  std::start_lifetime_as<pldm_msg>(resp.data()), sizeof(resp)));

    /* The completion code of the terminus is returned, so the polling task
     * does not take the read as successful
     */
    auto res = stdexec::sync_wait(sensorManager.getStateSensorReadings(sensor));
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(PLDM_PLATFORM_INVALID_SENSOR_ID, std::get<0>(*res));
}
