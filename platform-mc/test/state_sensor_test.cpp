
#include "libpldm/entity.h"
#include "libpldm/platform.h"

#include "platform-mc/state_sensor.hpp"
#include "platform-mc/terminus.hpp"

#include <gtest/gtest.h>

using namespace pldm::pdr;

TEST(StateSensor, processStateSensorReadings)
{
    pldm::pdr::CompositeCount comp_sensor_count = 2, comp_sensor_count_f = 3;
    pldm::pdr::EventState state = 1, state_f = 0, opState = PLDM_SENSOR_ENABLED;

    std::vector<uint8_t> pdr1{
        0x1,
        0x0,
        0x0,
        0x0,                   // record handle
        0x1,                   // PDRHeaderVersion
        PLDM_STATE_SENSOR_PDR, // PDRType
        0x0,
        0x0,                   // recordChangeNumber
        21,
        0,                     // dataLength
        0,
        0,                     // PLDMTerminusHandle
        0x2,
        0x0,                   // sensorID=2
        PLDM_ENTITY_SYS_FIRMWARE,
        0,                     // entityType=31
        20,
        0,                     // entityInstanceNumber=20
        0x4,
        0x0,                   // containerID=4
        PLDM_NO_INIT,          // sensorInit
        false,                 // sensorAuxiliaryNamesPDR
        2,                     // composite_sensor_count
        1,
        0,                     // state_set_id=1
        1,                     // possible_states_size=1
        0x1e,                  // states[]=[1,2,3,4]
        2,
        0,                     // state_set_id=2
        1,                     // possible_states_size=1
        0x1e,                  // states[]=[1,2,3,4]
    };

    pldm_tid_t tid = 0x01;
    auto pdr1_ptr = std::make_shared<std::vector<uint8_t>>(pdr1);
    std::string terminusName = "S0";
    std::vector<std::string> sensorNames = {"Test_Health_Sensor",
                                            "Test_Availability_Sensor"};
    std::string inventoryPath{"/xyz/openbmc_project/inventory/board/S0"};
    pldm::responder::events::StateSensorHandler stateSensorHandler(
        EVENTS_JSONS_DIR);

    pldm::platform_mc::StateSensor stateSensor =
        pldm::platform_mc::StateSensor(tid, terminusName, pdr1_ptr, sensorNames,
                                       inventoryPath, stateSensorHandler);

    EXPECT_EQ(stateSensor.compositeCount, comp_sensor_count);

    std::array<get_sensor_state_field,
               PLDM_STATE_SENSOR_MAX_COMPOSITE_SENSOR_COUNT>
        stateField = {
            {{
                 0, // sensor_op_state=PLDM_SENSOR_ENABLED
                 2, // present_state
                 1, // previous_state
                 1  // event_state
             },
             {
                 0, // sensor_op_state=PLDM_SENSOR_ENABLED
                 2, // present_state
                 1, // previous_state
                 1  // event_state
             },
             {},
             {},
             {},
             {},
             {},
             {}}};

    // Call process operational state for component sensor index 0
    auto rc = stateSensor.processOpState(opState, 0);
    EXPECT_EQ(rc, PLDM_SUCCESS);

    // Call process state for component sensor index 0
    rc = stateSensor.processSensorState(state_f, 0);
    EXPECT_EQ(
        rc,
        PLDM_ERROR_INVALID_DATA); // because state_f is not in possible states

    // Call process state for component sensor index 0
    rc = stateSensor.processSensorState(state, 0);
    EXPECT_EQ(rc, PLDM_ERROR); // because DBus is not available

    // Call process state for component sensor index 1
    rc = stateSensor.processSensorState(state, 1);
    EXPECT_EQ(rc,
              PLDM_ERROR); // because of initial PLDM_SENSOR_DISABLED op state

    // Call process operational state for component sensor index 1
    rc = stateSensor.processOpState(opState, 1);
    EXPECT_EQ(rc, PLDM_SUCCESS);

    // Call process state for component sensor index 1
    rc = stateSensor.processSensorState(state, 1);
    EXPECT_EQ(rc, PLDM_SUCCESS); // because event is not configured

    // Call process state sensor reading
    rc = stateSensor.processStateSensorReadings(stateField, comp_sensor_count);
    EXPECT_EQ(rc, PLDM_SUCCESS);

    // Call process state sensor reading with out-of-bound component sensor
    // count
    rc =
        stateSensor.processStateSensorReadings(stateField, comp_sensor_count_f);
    EXPECT_EQ(rc,
              PLDM_ERROR_INVALID_DATA); // comp_sensor_count_f > compositeCount
}
