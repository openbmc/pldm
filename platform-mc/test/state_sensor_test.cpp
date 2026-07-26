#include "common/utils.hpp"
#include "platform-mc/dbus_impl_fru.hpp"
#include "platform-mc/state_sensor.hpp"

#include <libpldm/entity.h>
#include <libpldm/state_set.h>

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

TEST(StateSensor, healthIsTheMappedStateSet)
{
    // The Health state set is published, so it names the state namespace of
    // the component sensor object path.
    EXPECT_EQ("health",
              pldm::platform_mc::getStateSetName(PLDM_STATE_SET_HEALTH_STATE));

    // No other state set is mapped yet - including the OEM range - so their
    // component sensors get no object.
    EXPECT_FALSE(
        pldm::platform_mc::getStateSetName(PLDM_STATE_SET_CONFIGURATION_STATE)
            .has_value());
    EXPECT_FALSE(pldm::platform_mc::getStateSetName(0x8000).has_value());
}

TEST(StateSensor, healthStateCollapsesOntoFunctional)
{
    // Normal is the only healthy state of the state set.
    EXPECT_TRUE(pldm::platform_mc::healthStateToFunctional(
        PLDM_STATE_SET_HEALTH_STATE_NORMAL));

    // Functional is a boolean, so the nine unhealthy values of the state set
    // all collapse onto false, the non-critical ones included.
    for (const auto state :
         {PLDM_STATE_SET_HEALTH_STATE_NON_CRITICAL,
          PLDM_STATE_SET_HEALTH_STATE_CRITICAL,
          PLDM_STATE_SET_HEALTH_STATE_FATAL,
          PLDM_STATE_SET_HEALTH_STATE_UPPER_NON_CRITICAL,
          PLDM_STATE_SET_HEALTH_STATE_LOWER_NON_CRITICAL,
          PLDM_STATE_SET_HEALTH_STATE_UPPER_CRITICAL,
          PLDM_STATE_SET_HEALTH_STATE_LOWER_CRITICAL,
          PLDM_STATE_SET_HEALTH_STATE_UPPER_FATAL,
          PLDM_STATE_SET_HEALTH_STATE_LOWER_FATAL})
    {
        EXPECT_FALSE(pldm::platform_mc::healthStateToFunctional(state));
    }

    // A component sensor without a state is not functional either: every PLDM
    // state set reserves state value 0.
    EXPECT_FALSE(pldm::platform_mc::healthStateToFunctional(0));
}

TEST(StateSensor, healthIsOnlyAssociatedWithAPermittedEndpoint)
{
    // The association endpoint is restricted to the inventory items whose
    // Redfish resource exposes a health property, which the processor and the
    // accelerator entity types are mapped to.
    EXPECT_TRUE(pldm::dbus_api::isHealthAssociationEndpoint(PLDM_ENTITY_PROC));
    EXPECT_TRUE(pldm::dbus_api::isHealthAssociationEndpoint(PLDM_ENTITY_GPU));
    EXPECT_TRUE(
        pldm::dbus_api::isHealthAssociationEndpoint(PLDM_ENTITY_ACCELERATOR));

    // The remaining entity types are mapped to an inventory item the
    // association does not accept, so a component sensor of such a terminus
    // reports its health unlinked.
    for (const auto entityType :
         {PLDM_ENTITY_BOARD, PLDM_ENTITY_SYS_BOARD, PLDM_ENTITY_CARD,
          PLDM_ENTITY_SYSTEM_CHASSIS, PLDM_ENTITY_MEMORY_MODULE,
          PLDM_ENTITY_FAN, PLDM_ENTITY_POWER_SUPPLY})
    {
        EXPECT_FALSE(pldm::dbus_api::isHealthAssociationEndpoint(entityType));
    }

    // A terminus whose entity type was not found falls back to a board too.
    EXPECT_FALSE(pldm::dbus_api::isHealthAssociationEndpoint(0));
}

TEST(StateSensor, createdObjectIsEnableable)
{
    // Every object created through createStateSensorObject() carries
    // xyz.openbmc_project.Object.Enable, and starts disabled.
    auto& bus = pldm::utils::DBusHandler::getBus();
    auto intf = pldm::platform_mc::createStateSensorObject(
        bus, "/xyz/openbmc_project/state/health/S0_Sensor_1");
    ASSERT_NE(nullptr, intf);
    EXPECT_FALSE(intf->enabled());
}

TEST(StateSensor, objectPathInStateSetNamespace)
{
    auto info = makeInfo(1, {{PLDM_STATE_SET_CONFIGURATION_STATE, {1, 2}},
                             {PLDM_STATE_SET_HEALTH_STATE, {1, 2, 3, 4}}});
    pldm::platform_mc::StateSensor sensor(0x01, info, 1, "health",
                                          "S0_Sensor_1_1", "", 0);

    // The component sensor's object path is the state set namespace plus the
    // component name, and it takes the composite sensor's sensor ID.
    EXPECT_EQ("/xyz/openbmc_project/state/health/S0_Sensor_1_1", sensor.path);
    EXPECT_EQ(1, sensor.sensorId);
    EXPECT_EQ(1, sensor.offset);

    // The object is published disabled until the component sensor has a state.
    EXPECT_FALSE(sensor.enabled());
    sensor.enabled(true);
    EXPECT_TRUE(sensor.enabled());
}

TEST(StateSensor, associatedWithTheMonitoredInventoryItem)
{
    // A terminus whose inventory item is a permitted endpoint gets the
    // component sensor linked to it, and reads its state as any other one.
    auto info = makeInfo(2, {{PLDM_STATE_SET_HEALTH_STATE, {1, 2}}});
    pldm::platform_mc::StateSensor sensor(
        0x01, info, 0, "health", "S0_Sensor_2",
        "/xyz/openbmc_project/inventory/system/board/S0", PLDM_ENTITY_GPU);

    sensor.updateReading(PLDM_SENSOR_ENABLED,
                         PLDM_STATE_SET_HEALTH_STATE_NORMAL);
    EXPECT_TRUE(sensor.enabled());
}

TEST(StateSensor, offsetOutOfRangeThrows)
{
    auto info = makeInfo(4, {{PLDM_STATE_SET_HEALTH_STATE, {1, 2}}});
    EXPECT_THROW(pldm::platform_mc::StateSensor(0x01, info, 1, "health",
                                                "S0_Sensor_4_1", "", 0),
                 sdbusplus::exception_t);
}

TEST(StateSensor, unmappedStateSetThrows)
{
    // A component sensor of an unmapped state set has no interface to publish
    // its state through, so no object is created for it.
    auto info = makeInfo(5, {{PLDM_STATE_SET_CONFIGURATION_STATE, {1, 2}}});
    EXPECT_THROW(pldm::platform_mc::StateSensor(0x01, info, 0, "health",
                                                "S0_Sensor_5", "", 0),
                 sdbusplus::exception_t);
}

TEST(StateSensor, readingDrivesEnabled)
{
    auto info = makeInfo(1, {{PLDM_STATE_SET_HEALTH_STATE, {1, 2}}});
    pldm::platform_mc::StateSensor sensor(0x01, info, 0, "health",
                                          "S0_Sensor_1", "", 0);

    // A component sensor only has a state while its own operational state is
    // enabled.
    sensor.updateReading(PLDM_SENSOR_ENABLED,
                         PLDM_STATE_SET_HEALTH_STATE_CRITICAL);
    EXPECT_EQ(PLDM_SENSOR_ENABLED, sensor.sensorOpState);
    EXPECT_EQ(PLDM_STATE_SET_HEALTH_STATE_CRITICAL, sensor.presentState);
    EXPECT_TRUE(sensor.enabled());

    sensor.updateReading(PLDM_SENSOR_FAILED,
                         PLDM_STATE_SET_HEALTH_STATE_NORMAL);
    EXPECT_EQ(PLDM_STATE_SET_HEALTH_STATE_NORMAL, sensor.presentState);
    EXPECT_FALSE(sensor.enabled());
}

TEST(StateSensor, readErrorClearsState)
{
    auto info = makeInfo(1, {{PLDM_STATE_SET_HEALTH_STATE, {1, 2}}});
    pldm::platform_mc::StateSensor sensor(0x01, info, 0, "health",
                                          "S0_Sensor_1_err", "", 0);

    sensor.updateReading(PLDM_SENSOR_ENABLED,
                         PLDM_STATE_SET_HEALTH_STATE_NORMAL);
    ASSERT_TRUE(sensor.enabled());

    // A failed read leaves the state unknown rather than stale.
    sensor.handleErrGetStateSensorReading();
    EXPECT_EQ(PLDM_SENSOR_UNAVAILABLE, sensor.sensorOpState);
    EXPECT_EQ(0, sensor.presentState);
    EXPECT_FALSE(sensor.enabled());
}
