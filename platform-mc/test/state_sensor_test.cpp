#include "platform-mc/state_sensor.hpp"

#include <libpldm/entity.h>
#include <libpldm/platform.h>

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

TEST(StateSensor, pathAndFunctional)
{
    auto info = makeInfo(1, {{1, {1, 2, 3}}, {3, {1, 2}}});
    pldm::platform_mc::StateSensor sensor(
        0x01, true, info, 0, "health", "Terminus_1_Sensor_1_Component_0",
        "/xyz/openbmc_project/inventory/system/board/Terminus_1");

    // Object lives in the state set namespace, one per component sensor
    EXPECT_EQ(
        "/xyz/openbmc_project/sensors/health/Terminus_1_Sensor_1_Component_0",
        sensor.path);

    // Constructed disabled, no reading yet
    EXPECT_FALSE(sensor.functional());
    EXPECT_FALSE(sensor.getReading().has_value());

    // Enabled -> functional, raw wire values stored
    sensor.updateReading(PLDM_SENSOR_ENABLED, 1, 2);
    auto reading = sensor.getReading();
    ASSERT_TRUE(reading.has_value());
    EXPECT_EQ(PLDM_SENSOR_ENABLED, reading->opState);
    EXPECT_EQ(1, reading->presentState);
    EXPECT_EQ(2, reading->previousState);
    EXPECT_TRUE(sensor.functional());

    // Any non-enabled opState -> not functional
    sensor.updateReading(PLDM_SENSOR_FAILED, 2, 1);
    EXPECT_FALSE(sensor.functional());
}

TEST(StateSensor, componentsAreIndependent)
{
    // A composite sensor repeating a state set creates one object per offset;
    // each drives its own Functional from its own opState.
    auto info = makeInfo(2, {{1, {1, 2}}, {1, {1, 2}}});
    pldm::platform_mc::StateSensor c0(
        0x01, true, info, 0, "health", "Terminus_1_Sensor_2_Component_0",
        "/xyz/openbmc_project/inventory/system/board/Terminus_1");
    pldm::platform_mc::StateSensor c1(
        0x01, true, info, 1, "health", "Terminus_1_Sensor_2_Component_1",
        "/xyz/openbmc_project/inventory/system/board/Terminus_1");

    c0.updateReading(PLDM_SENSOR_ENABLED, 1, 1);
    EXPECT_TRUE(c0.functional());
    EXPECT_FALSE(c1.functional());
    EXPECT_FALSE(c1.getReading().has_value());
}

TEST(StateSensor, offsetOutOfRangeThrows)
{
    auto info = makeInfo(4, {{1, {1, 2}}});
    EXPECT_THROW(pldm::platform_mc::StateSensor(
                     0x01, true, info, 1, "health",
                     "Terminus_1_Sensor_4_Component_1",
                     "/xyz/openbmc_project/inventory/system/board/Terminus_1"),
                 sdbusplus::exception_t);
}

TEST(StateSensor, handleErrGetStateSensorReadings)
{
    auto info = makeInfo(3, {{1, {1, 2}}});
    pldm::platform_mc::StateSensor sensor(
        0x01, true, info, 0, "health", "Terminus_1_Sensor_3_Component_0",
        "/xyz/openbmc_project/inventory/system/board/Terminus_1");

    sensor.updateReading(PLDM_SENSOR_ENABLED, 1, 2);
    EXPECT_TRUE(sensor.functional());
    ASSERT_TRUE(sensor.getReading().has_value());

    sensor.handleErrGetStateSensorReadings();
    EXPECT_FALSE(sensor.functional());
    EXPECT_FALSE(sensor.getReading().has_value());
}
