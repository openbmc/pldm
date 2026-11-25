#include "common/start_lifetime_as.hpp"
#include "common/types.hpp"
#include "mock_sensor_manager.hpp"
#include "mock_terminus_manager.hpp"
#include "platform-mc/sensor_manager.hpp"
#include "platform-mc/terminus_manager.hpp"
#include "test/test_instance_id.hpp"
#include "utils_test.hpp"

#include <sdeventplus/event.hpp>

#include <array>
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
        0x3f, // stateTransitionInterval=1.0
        0,
        0,
        0x80,
        0x3f,                          // updateInterval=1.0
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

class StateEffecterManagerTest : public testing::Test
{
  protected:
    StateEffecterManagerTest() :
        bus(pldm::utils::DBusHandler::getBus()),
        event(sdeventplus::Event::get_default()), instanceIdDb(),
        reqHandler(pldmTransport, event, instanceIdDb, false),
        mockTerminusManager(event, reqHandler, instanceIdDb, termini, nullptr),
        sensorManager(event, mockTerminusManager, termini, nullptr)
    {}

    /** @brief Discover a terminus with the power supply entity, and start its
     *         polling so the state effecter commands can be sent.
     */
    void discoverTerminus()
    {
        pldm::MctpInfo mctpInfo(10, "", "", 1, std::nullopt);
        auto mappedTid = mockTerminusManager.mapTid(mctpInfo);
        tid = mappedTid.value();

        /* The terminus manager drops a request to an endpoint which is not
         * known to be available
         */
        mockTerminusManager.updateMctpEndpointAvailability(mctpInfo, true);
        termini[tid] = std::make_shared<pldm::platform_mc::Terminus>(
            tid, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
        termini[tid]->pdrs.push_back(entityAuxNamesPdr);
        termini[tid]->pdrs.push_back(entityAssociationPdr);

        /* SetStateEffecterEnables is only sent to a terminus which supports
         * it
         */
        std::vector<uint8_t> pldmCmds(
            PLDM_MAX_TYPES * (PLDM_MAX_CMDS_PER_TYPE / 8));
        uint8_t cmd = PLDM_SET_STATE_EFFECTER_ENABLES;
        auto idx = PLDM_PLATFORM * (PLDM_MAX_CMDS_PER_TYPE / 8) + (cmd / 8);
        pldmCmds[idx] = pldmCmds[idx] | (1 << (cmd % 8));
        termini[tid]->setSupportedCommands(pldmCmds);

        termini[tid]->parseTerminusPDRs();
        sensorManager.startPolling(tid);
    }

    /** @brief The PLDM command of the request which was sent at @p index */
    uint8_t sentCommand(size_t index)
    {
        auto& request = mockTerminusManager.sentRequests.at(index);
        return std::start_lifetime_as<pldm_msg>(request.data())->hdr.command;
    }

    /** @brief The effecter ID which the request sent at @p index addresses,
     *         carried as the first field of the payload.
     */
    uint16_t sentEffecterId(size_t index)
    {
        auto& request = mockTerminusManager.sentRequests.at(index);
        return request.at(sizeof(pldm_msg_hdr)) |
               (request.at(sizeof(pldm_msg_hdr) + 1) << 8);
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

    // Entity Association PDR: the terminus entity contains the power supply
    std::vector<uint8_t> entityAssociationPdr{
        0x3, 0x0, 0x0,
        0x0,                         // record handle
        0x1,                         // PDRHeaderVersion
        PLDM_PDR_ENTITY_ASSOCIATION, // PDRType
        0x1,
        0x0,                         // recordChangeNumber
        0x10,
        0,                           // dataLength
        /* Entity Association PDR Data*/
        0x1,
        0x0, // containerID = 1
        0x0, // associationType physical
        0x80,
        0x0, // container entityType system software
        0x1,
        0x0, // container entity instance number = 1
        0x0,
        0x0, // container entity container ID = 0
        1,   // numberOfChildren
        PLDM_ENTITY_POWER_SUPPLY,
        0x0, // child entityType power supply
        0x1,
        0x0, // child entity instance number = 1
        0x1,
        0x0  // child entity container ID = 1
    };
};

/** @brief A state effecter of a composite of two effecters on the power
 *         supply entity of terminus @p tid
 *
 *  @param[in] tid - the terminus ID of the effecter
 *  @param[in] effecterId - the effecter ID the PDR reports
 *  @param[in] effecterInit - the `effecterInit` the PDR reports
 */
static std::shared_ptr<pldm::platform_mc::StateEffecter> makeStateEffecter(
    pldm_tid_t tid, uint16_t effecterId, uint8_t effecterInit)
{
    auto info = std::make_shared<pldm::platform_mc::StateEffecterInfo>();
    info->pdr.effecter_id = effecterId;
    info->pdr.effecter_init = effecterInit;
    info->pdr.composite_effecter_count = 2;
    info->componentInfo.emplace_back(1, pldm::pdr::PossibleStates{1, 2});
    info->componentInfo.emplace_back(3, pldm::pdr::PossibleStates{1, 2});

    auto stateSets = std::make_shared<pldm::platform_mc::StateSets>(
        "/xyz/openbmc_project/inventory/system/Effecter_Entity");
    return std::make_shared<pldm::platform_mc::StateEffecter>(
        tid, std::move(info), std::move(stateSets));
}

using EffecterEnableState = pldm::platform_mc::StateEffecter::EnableState;

TEST_F(StateEffecterManagerTest, setStateEffecterEnablesTest)
{
    discoverTerminus();
    auto effecter = makeStateEffecter(tid, 0x5, PLDM_ENABLE_EFFECTER);

    std::array<uint8_t, sizeof(pldm_msg_hdr) +
                            PLDM_PLATFORM_SET_STATE_EFFECTER_ENABLES_RESP_BYTES>
        resp{0x0, 0x02, PLDM_SET_STATE_EFFECTER_ENABLES, PLDM_SUCCESS};
    EXPECT_EQ(PLDM_SUCCESS,
              mockTerminusManager.enqueueResponse(
                  std::start_lifetime_as<pldm_msg>(resp.data()), sizeof(resp)));

    auto res =
        stdexec::sync_wait(sensorManager.setStateEffecterEnables(effecter));
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(PLDM_SUCCESS, std::get<0>(*res));
    EXPECT_EQ(EffecterEnableState::enabled, effecter->enableState);

    /* Every composite effecter is enabled with its event messages disabled */
    ASSERT_EQ(1, mockTerminusManager.sentRequests.size());
    EXPECT_EQ(PLDM_SET_STATE_EFFECTER_ENABLES, sentCommand(0));
    const std::vector<uint8_t> expectedPayload{
        0x5,
        0x0, // effecterID
        2,   // compositeEffecterCount
        PLDM_PLATFORM_SET_STATE_EFFECTER_ENABLES_ENABLED,
        PLDM_PLATFORM_SET_STATE_EFFECTER_ENABLES_DISABLE_EVENTS,
        PLDM_PLATFORM_SET_STATE_EFFECTER_ENABLES_ENABLED,
        PLDM_PLATFORM_SET_STATE_EFFECTER_ENABLES_DISABLE_EVENTS};
    auto& request = mockTerminusManager.sentRequests.at(0);
    EXPECT_EQ(expectedPayload,
              std::vector<uint8_t>(request.begin() + sizeof(pldm_msg_hdr),
                                   request.end()));
}

TEST_F(StateEffecterManagerTest, setStateEffecterEnablesErrorTest)
{
    discoverTerminus();
    auto effecter = makeStateEffecter(tid, 0x5, PLDM_ENABLE_EFFECTER);

    std::array<uint8_t, sizeof(pldm_msg_hdr) +
                            PLDM_PLATFORM_SET_STATE_EFFECTER_ENABLES_RESP_BYTES>
        resp{0x0, 0x02, PLDM_SET_STATE_EFFECTER_ENABLES, PLDM_ERROR};
    EXPECT_EQ(PLDM_SUCCESS,
              mockTerminusManager.enqueueResponse(
                  std::start_lifetime_as<pldm_msg>(resp.data()), sizeof(resp)));

    auto res =
        stdexec::sync_wait(sensorManager.setStateEffecterEnables(effecter));
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(PLDM_ERROR, std::get<0>(*res));

    /* The terminus answered, so the effecter is not enabled again */
    EXPECT_EQ(EffecterEnableState::rejected, effecter->enableState);
}

TEST_F(StateEffecterManagerTest, pollingEnablesEffecterOnceTest)
{
    discoverTerminus();
    auto enableEffecter = makeStateEffecter(tid, 0x5, PLDM_ENABLE_EFFECTER);
    auto noInitEffecter = makeStateEffecter(tid, 0x6, PLDM_NO_INIT);
    termini[tid]->stateEffecters.push_back(enableEffecter);
    termini[tid]->stateEffecters.push_back(noInitEffecter);

    std::array<uint8_t, sizeof(pldm_msg_hdr) +
                            PLDM_PLATFORM_SET_STATE_EFFECTER_ENABLES_RESP_BYTES>
        resp{0x0, 0x02, PLDM_SET_STATE_EFFECTER_ENABLES, PLDM_SUCCESS};
    EXPECT_EQ(PLDM_SUCCESS,
              mockTerminusManager.enqueueResponse(
                  std::start_lifetime_as<pldm_msg>(resp.data()), sizeof(resp)));

    auto res = stdexec::sync_wait(sensorManager.doSensorPollingTask(tid));
    ASSERT_TRUE(res.has_value());

    /* Only the effecter whose PDR asks for it is enabled */
    ASSERT_EQ(1, mockTerminusManager.sentRequests.size());
    EXPECT_EQ(PLDM_SET_STATE_EFFECTER_ENABLES, sentCommand(0));
    EXPECT_EQ(0x5, sentEffecterId(0));
    EXPECT_EQ(EffecterEnableState::enabled, enableEffecter->enableState);
    EXPECT_EQ(EffecterEnableState::pending, noInitEffecter->enableState);

    /* An enabled effecter is not enabled again on the next pass */
    res = stdexec::sync_wait(sensorManager.doSensorPollingTask(tid));
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(1, mockTerminusManager.sentRequests.size());
}
