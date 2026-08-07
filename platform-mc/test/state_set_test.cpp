#include "common/utils.hpp"
#include "platform-mc/state_set.hpp"

#include <libpldm/state_set.h>

#include <array>
#include <string>

#include <gtest/gtest.h>

using namespace pldm::platform_mc;

TEST(StateSetTest, createStateSetTest)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string path = "/xyz/openbmc_project/inventory/test/health_state";

    /* The health state set has a D-Bus interface */
    EXPECT_NE(nullptr, createStateSet(bus, path, PLDM_STATE_SET_HEALTH_STATE));

    /* A state set whose interface is not added yet has none */
    EXPECT_EQ(nullptr, createStateSet(bus, path, PLDM_STATE_SET_PRESENCE));
    EXPECT_EQ(nullptr,
              createStateSet(bus, path, PLDM_STATE_SET_CONFIGURATION_STATE));
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

TEST(StateSetTest, stateSetsShareOneInterfaceTest)
{
    StateSets stateSets("/xyz/openbmc_project/inventory/test/health_shared");

    /* The component sensors which report the same state set of the same
     * entity share one interface
     */
    auto* first = stateSets.getStateSet(PLDM_STATE_SET_HEALTH_STATE);
    ASSERT_NE(nullptr, first);
    EXPECT_EQ(first, stateSets.getStateSet(PLDM_STATE_SET_HEALTH_STATE));

    /* A state set with no D-Bus interface publishes nothing */
    EXPECT_EQ(nullptr, stateSets.getStateSet(PLDM_STATE_SET_PRESENCE));
}
