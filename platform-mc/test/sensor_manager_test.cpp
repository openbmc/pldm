#include "common/start_lifetime_as.hpp"
#include "common/types.hpp"
#include "mock_sensor_manager.hpp"
#include "mock_terminus_manager.hpp"
#include "platform-mc/sensor_manager.hpp"
#include "platform-mc/terminus_manager.hpp"
#include "test/test_instance_id.hpp"
#include "utils_test.hpp"

#include <sdeventplus/event.hpp>

#include <format>

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

class StateSensorPollingTest : public testing::Test
{
  protected:
    StateSensorPollingTest() :
        bus(pldm::utils::DBusHandler::getBus()),
        event(sdeventplus::Event::get_default()), instanceIdDb(),
        reqHandler(pldmTransport, event, instanceIdDb, false),
        mockTerminusManager(event, reqHandler, instanceIdDb, termini, nullptr),
        sensorManager(event, mockTerminusManager, termini, nullptr)
    {}

    /** @brief A composite state sensor with @p count offsets and a component
     *         sensor object for each of them.
     */
    std::vector<std::shared_ptr<pldm::platform_mc::StateSensor>> makeComponents(
        pldm_tid_t tid, uint16_t sensorId, uint8_t count)
    {
        auto info = std::make_shared<pldm::platform_mc::StateSensorInfo>();
        info->pdr.sensor_id = sensorId;
        info->pdr.composite_sensor_count = count;
        for (uint8_t offset = 0; offset < count; offset++)
        {
            info->compositeInfo.emplace_back(offset + 1,
                                             std::set<uint8_t>{1, 2});
        }

        std::vector<std::shared_ptr<pldm::platform_mc::StateSensor>> components;
        for (uint8_t offset = 0; offset < count; offset++)
        {
            components.emplace_back(
                std::make_shared<pldm::platform_mc::StateSensor>(
                    tid, info, offset, "test_state_set",
                    std::format("S0_Sensor_{}_{}", sensorId, offset)));
        }
        return components;
    }

    PldmTransport* pldmTransport = nullptr;
    sdbusplus::bus_t& bus;
    sdeventplus::Event event;
    TestInstanceIdDb instanceIdDb;
    pldm::requester::Handler<pldm::requester::Request> reqHandler;
    pldm::platform_mc::MockTerminusManager mockTerminusManager;
    pldm::platform_mc::MockSensorManager sensorManager;
    std::map<pldm_tid_t, std::shared_ptr<pldm::platform_mc::Terminus>> termini;
};

TEST_F(StateSensorPollingTest, stateSensorReadingsFanOutTest)
{
    auto mappedTid =
        mockTerminusManager.mapTid(pldm::MctpInfo(10, "", "", 1, std::nullopt));
    auto tid = mappedTid.value();
    termini[tid] = std::make_shared<pldm::platform_mc::Terminus>(
        tid, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
    mockTerminusManager.updateMctpEndpointAvailability(
        pldm::MctpInfo(10, "", "", 1, std::nullopt), true);
    auto components = makeComponents(tid, 1, 2);
    sensorManager.startPolling(tid);

    // One response for sensor ID 1 carrying the state of both offsets.
    std::array<uint8_t, sizeof(pldm_msg_hdr) + 10> resp{
        0x0,
        0x02,
        0x21, // response header
        PLDM_SUCCESS,
        2,    // compositeSensorCount
        PLDM_SENSOR_ENABLED,
        2,
        1,
        2, // offset 0 state field
        PLDM_SENSOR_DISABLED,
        1,
        1,
        1 // offset 1 state field
    };
    EXPECT_EQ(PLDM_SUCCESS,
              mockTerminusManager.enqueueResponse(
                  std::start_lifetime_as<pldm_msg>(resp.data()), sizeof(resp)));

    stdexec::sync_wait(
        sensorManager.callGetStateSensorReadings(tid, 1, components));

    // Each component sensor takes the state field of its own offset, and is
    // enabled from its own operational state.
    EXPECT_EQ(PLDM_SENSOR_ENABLED, components[0]->sensorOpState);
    EXPECT_EQ(2, components[0]->presentState);
    EXPECT_TRUE(components[0]->enabled());

    EXPECT_EQ(PLDM_SENSOR_DISABLED, components[1]->sensorOpState);
    EXPECT_EQ(1, components[1]->presentState);
    EXPECT_FALSE(components[1]->enabled());

    sensorManager.stopPolling(tid);
}

TEST_F(StateSensorPollingTest, stateSensorReadingsShortResponseTest)
{
    auto mappedTid =
        mockTerminusManager.mapTid(pldm::MctpInfo(10, "", "", 1, std::nullopt));
    auto tid = mappedTid.value();
    termini[tid] = std::make_shared<pldm::platform_mc::Terminus>(
        tid, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
    mockTerminusManager.updateMctpEndpointAvailability(
        pldm::MctpInfo(10, "", "", 1, std::nullopt), true);
    auto components = makeComponents(tid, 2, 2);
    sensorManager.startPolling(tid);

    // The terminus answers with fewer offsets than the PDR described.
    std::array<uint8_t, sizeof(pldm_msg_hdr) + 6> resp{
        0x0,
        0x02,
        0x21, // response header
        PLDM_SUCCESS,
        1,    // compositeSensorCount
        PLDM_SENSOR_ENABLED,
        2,
        1,
        2 // offset 0 state field
    };
    EXPECT_EQ(PLDM_SUCCESS,
              mockTerminusManager.enqueueResponse(
                  std::start_lifetime_as<pldm_msg>(resp.data()), sizeof(resp)));

    stdexec::sync_wait(
        sensorManager.callGetStateSensorReadings(tid, 2, components));

    // The covered offset takes its state; the uncovered one has none.
    EXPECT_EQ(2, components[0]->presentState);
    EXPECT_TRUE(components[0]->enabled());

    EXPECT_EQ(PLDM_SENSOR_UNAVAILABLE, components[1]->sensorOpState);
    EXPECT_EQ(0, components[1]->presentState);
    EXPECT_FALSE(components[1]->enabled());

    sensorManager.stopPolling(tid);
}

TEST_F(StateSensorPollingTest, stateSensorReadingsFailureTest)
{
    auto mappedTid =
        mockTerminusManager.mapTid(pldm::MctpInfo(10, "", "", 1, std::nullopt));
    auto tid = mappedTid.value();
    termini[tid] = std::make_shared<pldm::platform_mc::Terminus>(
        tid, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
    mockTerminusManager.updateMctpEndpointAvailability(
        pldm::MctpInfo(10, "", "", 1, std::nullopt), true);
    auto components = makeComponents(tid, 3, 2);
    sensorManager.startPolling(tid);

    // Take a state first, so the failure below has something to clear.
    components[0]->updateReading(PLDM_SENSOR_ENABLED, 2);
    EXPECT_TRUE(components[0]->enabled());

    // No response is queued, so the request fails.
    stdexec::sync_wait(
        sensorManager.callGetStateSensorReadings(tid, 3, components));

    for (const auto& component : components)
    {
        EXPECT_EQ(PLDM_SENSOR_UNAVAILABLE, component->sensorOpState);
        EXPECT_EQ(0, component->presentState);
        EXPECT_FALSE(component->enabled());
    }

    sensorManager.stopPolling(tid);
}
