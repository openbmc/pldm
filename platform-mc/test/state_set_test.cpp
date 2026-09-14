#include "common/utils.hpp"
#include "platform-mc/state_set.hpp"
#include "platform-mc/state_set_presence.hpp"

#include <libpldm/state_set.h>

#include <sdbusplus/server/object.hpp>

#include <memory>
#include <string>

#include <gtest/gtest.h>

using namespace pldm::platform_mc;

using InventoryItemIntf = sdbusplus::server::object_t<InventoryItemServer>;

TEST(StateSetTest, createStateSetTest)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string path = "/xyz/openbmc_project/inventory/test/state_set";
    auto itemIntf = std::make_shared<InventoryItemIntf>(bus, path.c_str());

    /* The presence state set has a D-Bus interface */
    EXPECT_NE(nullptr,
              createStateSet(bus, path, itemIntf, PLDM_STATE_SET_PRESENCE));

    /* A state set whose interface is not added yet has none */
    EXPECT_EQ(nullptr, createStateSet(bus, path, itemIntf,
                                      PLDM_STATE_SET_CONFIGURATION_STATE));
}

TEST(StateSetTest, presencePresentTest)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string path = "/xyz/openbmc_project/inventory/test/presence_present";
    auto itemIntf = std::make_shared<InventoryItemIntf>(bus, path.c_str());
    StateSetPresence stateSet(path, itemIntf);

    /* Each reading replaces the previous one, so the entity comes back when
     * the terminus reports it present again
     */
    stateSet.setPresentState(PLDM_STATE_SET_PRESENCE_NOT_PRESENT);
    EXPECT_EQ(false, stateSet.present());

    stateSet.setPresentState(PLDM_STATE_SET_PRESENCE_PRESENT);
    EXPECT_EQ(true, stateSet.present());

    stateSet.setPresentState(PLDM_STATE_SET_PRESENCE_NOT_PRESENT);
    EXPECT_EQ(false, stateSet.present());
}

TEST(StateSetTest, presenceUnknownStateTest)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string path = "/xyz/openbmc_project/inventory/test/presence_unknown";
    auto itemIntf = std::make_shared<InventoryItemIntf>(bus, path.c_str());
    StateSetPresence stateSet(path, itemIntf);

    /* A state the state set does not define keeps the presence of the last
     * reading a state value was defined for
     */
    stateSet.setPresentState(PLDM_STATE_SET_PRESENCE_PRESENT);
    stateSet.setPresentState(0xff);
    EXPECT_EQ(true, stateSet.present());

    stateSet.setPresentState(PLDM_STATE_SET_PRESENCE_NOT_PRESENT);
    stateSet.setPresentState(0xff);
    EXPECT_EQ(false, stateSet.present());
}

TEST(StateSetTest, stateSetsShareOneInterfaceTest)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string path = "/xyz/openbmc_project/inventory/test/state_set_shared";
    auto itemIntf = std::make_shared<InventoryItemIntf>(bus, path.c_str());
    StateSets stateSets(path, itemIntf);

    /* The component sensors which report the same state set of the same
     * entity share one interface
     */
    auto* presence = stateSets.getStateSet(PLDM_STATE_SET_PRESENCE);
    ASSERT_NE(nullptr, presence);
    EXPECT_EQ(presence, stateSets.getStateSet(PLDM_STATE_SET_PRESENCE));

    /* The presence state set takes the Inventory.Item interface of the entity
     * instead of implementing a second one
     */
    presence->setPresentState(PLDM_STATE_SET_PRESENCE_NOT_PRESENT);
    EXPECT_EQ(false, itemIntf->present());

    presence->setPresentState(PLDM_STATE_SET_PRESENCE_PRESENT);
    EXPECT_EQ(true, itemIntf->present());

    /* A state set with no D-Bus interface publishes nothing */
    EXPECT_EQ(nullptr,
              stateSets.getStateSet(PLDM_STATE_SET_CONFIGURATION_STATE));
}
