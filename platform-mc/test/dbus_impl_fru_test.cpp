#include "common/utils.hpp"
#include "platform-mc/dbus_impl_fru.hpp"

#include <libpldm/entity.h>

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

using namespace pldm::dbus_api;

namespace
{

/** @brief An entity type which has no matching Inventory.Item interface */
constexpr uint16_t unmappedEntityType = 0xFFFF;

std::string objectPath(const std::string& name)
{
    return "/xyz/openbmc_project/inventory/system/test_" + name;
}

} // namespace

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

    /* An entity type which has no matching interface falls back to the
     * Inventory.Item.Board interface.
     */
    auto fallback =
        createPldmEntity(bus, objectPath("fallback"), unmappedEntityType);
    EXPECT_NE(nullptr,
              dynamic_cast<PldmEntityReq<BoardServer>*>(fallback.get()));
}
