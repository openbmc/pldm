#include "common/utils.hpp"
#include "platform-mc/dbus_impl_fru.hpp"

#include <libpldm/entity.h>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

using namespace pldm::dbus_api;

namespace
{

/** @brief An entity type which the entity type table does not cover */
constexpr uint16_t unmappedEntityType = 0xFFFF;

std::string objectPath(const std::string& name)
{
    return "/xyz/openbmc_project/inventory/system/test_" + name;
}

} // namespace

TEST(DbusImplFruTest, getPldmEntityNameTest)
{
    auto cpu = getPldmEntityName(PLDM_ENTITY_PROC);
    ASSERT_TRUE(cpu.has_value());
    EXPECT_EQ("Cpu", *cpu);

    auto sysBoard = getPldmEntityName(PLDM_ENTITY_SYS_BOARD);
    ASSERT_TRUE(sysBoard.has_value());
    EXPECT_EQ("SysBoard", *sysBoard);

    EXPECT_FALSE(getPldmEntityName(unmappedEntityType).has_value());
}

TEST(DbusImplFruTest, entityTypeTableIsUniqueTest)
{
    /* The name identifies the entity type in a D-Bus object name, so two
     * entity types share neither a name nor a table entry.
     */
    std::set<uint16_t> types;
    std::set<std::string_view> names;
    for (const auto& item : pldmEntityItems)
    {
        EXPECT_FALSE(item.name.empty());
        EXPECT_TRUE(types.insert(item.entityType).second) << item.name;
        EXPECT_TRUE(names.insert(item.name).second) << item.name;
    }

    EXPECT_EQ(pldmEntityItems.size(), types.size());
    EXPECT_EQ(pldmEntityItems.size(), names.size());
}

TEST(DbusImplFruTest, sharedItemInterfaceKeepsOwnNameTest)
{
    auto gpu = getPldmEntityName(PLDM_ENTITY_GPU);
    auto accelerator = getPldmEntityName(PLDM_ENTITY_ACCELERATOR);
    ASSERT_TRUE(gpu.has_value());
    ASSERT_TRUE(accelerator.has_value());

    EXPECT_NE(*gpu, *accelerator);
}

TEST(DbusImplFruTest, createPldmEntityForTypeTest)
{
    auto& bus = pldm::utils::DBusHandler::getBus();

    auto cpu =
        createPldmEntityForType(bus, objectPath("cpu"), PLDM_ENTITY_PROC);
    EXPECT_NE(nullptr, dynamic_cast<PldmEntityReq<CpuServer>*>(cpu.get()));

    /* The two entity types which share the Inventory.Item.Accelerator
     * interface both create it.
     */
    auto gpu = createPldmEntityForType(bus, objectPath("gpu"), PLDM_ENTITY_GPU);
    EXPECT_NE(nullptr,
              dynamic_cast<PldmEntityReq<AcceleratorServer>*>(gpu.get()));

    auto accelerator = createPldmEntityForType(bus, objectPath("accelerator"),
                                               PLDM_ENTITY_ACCELERATOR);
    EXPECT_NE(nullptr, dynamic_cast<PldmEntityReq<AcceleratorServer>*>(
                           accelerator.get()));

    auto unmapped = createPldmEntityForType(bus, objectPath("unmapped"),
                                            unmappedEntityType);
    EXPECT_EQ(nullptr, unmapped);
}

TEST(DbusImplFruTest, createPldmEntityTest)
{
    auto& bus = pldm::utils::DBusHandler::getBus();

    auto powerSupply = createPldmEntity(bus, objectPath("power_supply"),
                                        PLDM_ENTITY_POWER_SUPPLY);
    EXPECT_NE(nullptr, dynamic_cast<PldmEntityReq<PowerSupplyServer>*>(
                           powerSupply.get()));

    /* An entity type which the table does not cover falls back to the
     * Inventory.Item.Board interface.
     */
    auto fallback =
        createPldmEntity(bus, objectPath("fallback"), unmappedEntityType);
    EXPECT_NE(nullptr,
              dynamic_cast<PldmEntityReq<BoardServer>*>(fallback.get()));
}
