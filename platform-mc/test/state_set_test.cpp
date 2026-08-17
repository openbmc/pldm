#include "common/utils.hpp"
#include "platform-mc/state_set.hpp"

#include <libpldm/state_set.h>

#include <array>
#include <memory>
#include <string>

#include <gtest/gtest.h>

using namespace pldm::platform_mc;

TEST(StateSetTest, createStateSetTest)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string path = "/xyz/openbmc_project/inventory/test/state_set";
    auto itemIntf = std::make_shared<InventoryItemIntf>(bus, path.c_str());

    /* The health state set and the presence state set have a D-Bus interface
     */
    EXPECT_NE(nullptr,
              createStateSet(bus, path, itemIntf, PLDM_STATE_SET_HEALTH_STATE));
    EXPECT_NE(nullptr,
              createStateSet(bus, path, itemIntf, PLDM_STATE_SET_PRESENCE));

    /* A state set whose interface is not added yet has none */
    EXPECT_EQ(nullptr, createStateSet(bus, path, itemIntf,
                                      PLDM_STATE_SET_CONFIGURATION_STATE));
}

TEST(StateSetTest, healthStateFunctionalTest)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    StateSetHealthState stateSet(
        bus, "/xyz/openbmc_project/inventory/test/health_functional");

    struct TestCase
    {
        uint8_t presentState;
        bool functional;
    };

    /* A state which reports a condition short of critical leaves the entity
     * functional, and a state the state set does not define does not
     */
    // clang-format off
    std::array<TestCase, 11> testCases{{
        {PLDM_STATE_SET_HEALTH_STATE_NORMAL,             true},
        {PLDM_STATE_SET_HEALTH_STATE_NON_CRITICAL,       true},
        {PLDM_STATE_SET_HEALTH_STATE_UPPER_NON_CRITICAL, true},
        {PLDM_STATE_SET_HEALTH_STATE_LOWER_NON_CRITICAL, true},
        {PLDM_STATE_SET_HEALTH_STATE_CRITICAL,           false},
        {PLDM_STATE_SET_HEALTH_STATE_UPPER_CRITICAL,     false},
        {PLDM_STATE_SET_HEALTH_STATE_LOWER_CRITICAL,     false},
        {PLDM_STATE_SET_HEALTH_STATE_FATAL,              false},
        {PLDM_STATE_SET_HEALTH_STATE_UPPER_FATAL,        false},
        {PLDM_STATE_SET_HEALTH_STATE_LOWER_FATAL,        false},
        {0xff,                                           false},
    }};
    // clang-format on

    for (const auto& testCase : testCases)
    {
        stateSet.setPresentState(testCase.presentState);
        EXPECT_EQ(testCase.functional, stateSet.functional())
            << "presentState " << static_cast<int>(testCase.presentState);
    }
}

TEST(StateSetTest, healthStateTransitionTest)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    StateSetHealthState stateSet(
        bus, "/xyz/openbmc_project/inventory/test/health_transition");

    /* Each reading replaces the previous one, so the entity recovers when the
     * terminus stops reporting a critical state
     */
    stateSet.setPresentState(PLDM_STATE_SET_HEALTH_STATE_FATAL);
    EXPECT_EQ(false, stateSet.functional());

    stateSet.setPresentState(PLDM_STATE_SET_HEALTH_STATE_NORMAL);
    EXPECT_EQ(true, stateSet.functional());

    stateSet.setPresentState(PLDM_STATE_SET_HEALTH_STATE_CRITICAL);
    EXPECT_EQ(false, stateSet.functional());
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
    auto* first = stateSets.getStateSet(PLDM_STATE_SET_HEALTH_STATE);
    ASSERT_NE(nullptr, first);
    EXPECT_EQ(first, stateSets.getStateSet(PLDM_STATE_SET_HEALTH_STATE));

    /* The presence state set takes the Inventory.Item interface of the entity
     * instead of implementing a second one
     */
    auto* presence = stateSets.getStateSet(PLDM_STATE_SET_PRESENCE);
    ASSERT_NE(nullptr, presence);
    presence->setPresentState(PLDM_STATE_SET_PRESENCE_NOT_PRESENT);
    EXPECT_EQ(false, itemIntf->present());

    /* A state set with no D-Bus interface publishes nothing */
    EXPECT_EQ(nullptr,
              stateSets.getStateSet(PLDM_STATE_SET_CONFIGURATION_STATE));
}
