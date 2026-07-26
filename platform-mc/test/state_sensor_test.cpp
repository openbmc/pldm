#include "common/utils.hpp"
#include "platform-mc/state_sensor.hpp"

#include <libpldm/entity.h>

#include <gtest/gtest.h>

static std::shared_ptr<pldm::platform_mc::StateSensorInfo> makeInfo(
    uint16_t sensorId,
    std::vector<std::pair<uint16_t, std::set<uint8_t>>> compositeInfo)
{
    auto info = std::make_shared<pldm::platform_mc::StateSensorInfo>();
    info->pdr.sensor_id = sensorId;
    info->pdr.entity_type = PLDM_ENTITY_POWER_SUPPLY;
    info->pdr.entity_instance_number = 1;
    info->pdr.container_id = 1;
    info->pdr.composite_sensor_count = compositeInfo.size();
    info->compositeInfo = std::move(compositeInfo);
    return info;
}

TEST(StateSensor, unmappedStateSetHasNoName)
{
    // No state set is mapped to a name yet, so no state set - including the
    // OEM range - resolves to one.
    EXPECT_FALSE(pldm::platform_mc::getStateSetName(1).has_value());
    EXPECT_FALSE(pldm::platform_mc::getStateSetName(3).has_value());
    EXPECT_FALSE(pldm::platform_mc::getStateSetName(0x8000).has_value());
}

TEST(StateSensor, createdObjectIsEnableable)
{
    // Every object created through createStateSensorObject() carries
    // xyz.openbmc_project.Object.Enable, and starts disabled.
    auto& bus = pldm::utils::DBusHandler::getBus();
    auto intf = pldm::platform_mc::createStateSensorObject(
        bus, "/xyz/openbmc_project/state/test_state_set/S0_Sensor_1");
    ASSERT_NE(nullptr, intf);
    EXPECT_FALSE(intf->enabled());
}

TEST(StateSensor, objectPathInStateSetNamespace)
{
    auto info = makeInfo(1, {{1, {1, 2}}, {3, {1, 2, 3, 4}}});
    pldm::platform_mc::StateSensor sensor(0x01, info, 1, "test_state_set",
                                          "S0_Sensor_1_1");

    // The component sensor's object path is the state set namespace plus the
    // component name, and it takes the composite sensor's sensor ID.
    EXPECT_EQ("/xyz/openbmc_project/state/test_state_set/S0_Sensor_1_1",
              sensor.path);
    EXPECT_EQ(1, sensor.sensorId);
    EXPECT_EQ(1, sensor.offset);

    // The object is published disabled until the component sensor has a state.
    EXPECT_FALSE(sensor.enabled());
    sensor.enabled(true);
    EXPECT_TRUE(sensor.enabled());
}

TEST(StateSensor, offsetOutOfRangeThrows)
{
    auto info = makeInfo(4, {{1, {1, 2}}});
    EXPECT_THROW(pldm::platform_mc::StateSensor(0x01, info, 1, "test_state_set",
                                                "S0_Sensor_4_1"),
                 sdbusplus::exception_t);
}

TEST(StateSensor, readingDrivesEnabled)
{
    auto info = makeInfo(1, {{1, {1, 2}}});
    pldm::platform_mc::StateSensor sensor(0x01, info, 0, "test_state_set",
                                          "S0_Sensor_1");

    // A component sensor only has a state while its own operational state is
    // enabled.
    sensor.updateReading(PLDM_SENSOR_ENABLED, 2);
    EXPECT_EQ(PLDM_SENSOR_ENABLED, sensor.sensorOpState);
    EXPECT_EQ(2, sensor.presentState);
    EXPECT_TRUE(sensor.enabled());

    sensor.updateReading(PLDM_SENSOR_FAILED, 1);
    EXPECT_EQ(1, sensor.presentState);
    EXPECT_FALSE(sensor.enabled());
}

TEST(StateSensor, readErrorClearsState)
{
    auto info = makeInfo(1, {{1, {1, 2}}});
    pldm::platform_mc::StateSensor sensor(0x01, info, 0, "test_state_set",
                                          "S0_Sensor_1_err");

    sensor.updateReading(PLDM_SENSOR_ENABLED, 2);
    ASSERT_TRUE(sensor.enabled());

    // A failed read leaves the state unknown rather than stale.
    sensor.handleErrGetStateSensorReading();
    EXPECT_EQ(PLDM_SENSOR_UNAVAILABLE, sensor.sensorOpState);
    EXPECT_EQ(0, sensor.presentState);
    EXPECT_FALSE(sensor.enabled());
}
