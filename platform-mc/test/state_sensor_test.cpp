#include "platform-mc/state_sensor.hpp"
#include "platform-mc/state_set.hpp"

#include <memory>

#include <gtest/gtest.h>

namespace
{

constexpr pldm::pdr::StateSetId firstStateSetId = 1;
constexpr pldm::pdr::StateSetId secondStateSetId = 3;

std::shared_ptr<pldm::platform_mc::StateSensorInfo> makeSensorInfo()
{
    auto info = std::make_shared<pldm::platform_mc::StateSensorInfo>();
    info->pdr.sensor_id = 1;
    info->pdr.composite_sensor_count = 2;
    info->componentInfo.emplace_back(firstStateSetId,
                                     pldm::pdr::PossibleStates{1, 2});
    info->componentInfo.emplace_back(secondStateSetId,
                                     pldm::pdr::PossibleStates{1, 2});
    return info;
}

} // namespace

TEST(StateSensor, controlledByEffectersWhenEveryStateSetIsControlled)
{
    auto stateSets = std::make_shared<pldm::platform_mc::StateSets>(
        "/xyz/openbmc_project/inventory/system/Entity_1");
    stateSets->addEffecterStateSet(firstStateSetId);
    stateSets->addEffecterStateSet(secondStateSetId);

    pldm::platform_mc::StateSensor sensor(1, makeSensorInfo(), stateSets);
    EXPECT_TRUE(sensor.isControlledByEffecters());
}

TEST(StateSensor, notControlledByEffectersWhenOneStateSetIsLeft)
{
    auto stateSets = std::make_shared<pldm::platform_mc::StateSets>(
        "/xyz/openbmc_project/inventory/system/Entity_2");
    stateSets->addEffecterStateSet(firstStateSetId);

    pldm::platform_mc::StateSensor sensor(1, makeSensorInfo(), stateSets);
    EXPECT_FALSE(sensor.isControlledByEffecters());
}

TEST(StateSensor, notControlledByEffectersWithoutEffecters)
{
    auto stateSets = std::make_shared<pldm::platform_mc::StateSets>(
        "/xyz/openbmc_project/inventory/system/Entity_3");

    pldm::platform_mc::StateSensor sensor(1, makeSensorInfo(), stateSets);
    EXPECT_FALSE(sensor.isControlledByEffecters());
}
