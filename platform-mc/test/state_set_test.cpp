#include "platform-mc/state_set.hpp"
#include "platform-mc/state_set_health_state.hpp"

#include <libpldm/state_set.h>

#include <array>
#include <string>

#include <gtest/gtest.h>

using namespace pldm::platform_mc;

TEST(StateSets, shareInterface)
{
    StateSets stateSets("/xyz/openbmc_project/inventory/system/Entity_1");

    auto& first = stateSets.getInterface<OperationalStatusIntf>();
    auto& second = stateSets.getInterface<OperationalStatusIntf>();
    EXPECT_EQ(&first, &second);
}

TEST(StateSets, initInterfaceOnce)
{
    StateSets stateSets("/xyz/openbmc_project/inventory/system/Entity_2");

    auto& intf = stateSets.getInterface<OperationalStatusIntf>(
        [](OperationalStatusIntf& i) { i.functional(true); });
    EXPECT_TRUE(intf.functional());

    stateSets.getInterface<OperationalStatusIntf>([](OperationalStatusIntf& i) {
        i.functional(false);
    });
    EXPECT_TRUE(intf.functional());
}

TEST(StateSetTest, createStateSetTest)
{
    StateSets stateSets("/xyz/openbmc_project/inventory/test/health_state");

    /* The health state set has a D-Bus interface */
    EXPECT_NE(nullptr, createStateSet(stateSets, PLDM_STATE_SET_HEALTH_STATE));

    /* A state set whose interface is not added yet has none */
    EXPECT_EQ(nullptr, createStateSet(stateSets, PLDM_STATE_SET_PRESENCE));
    EXPECT_EQ(nullptr,
              createStateSet(stateSets, PLDM_STATE_SET_CONFIGURATION_STATE));
}

TEST(StateSetTest, healthStateFunctionalTest)
{
    StateSets stateSets(
        "/xyz/openbmc_project/inventory/test/health_functional");
    StateSetHealthState stateSet(stateSets);

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
    StateSets stateSets(
        "/xyz/openbmc_project/inventory/test/health_transition");
    StateSetHealthState stateSet(stateSets);

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

    /* The health state set publishes on the OperationalStatus interface which
     * the other state sets of the entity share
     */
    first->setPresentState(PLDM_STATE_SET_HEALTH_STATE_FATAL);
    EXPECT_FALSE(stateSets.getInterface<OperationalStatusIntf>().functional());

    /* A state set with no D-Bus interface publishes nothing */
    EXPECT_EQ(nullptr, stateSets.getStateSet(PLDM_STATE_SET_PRESENCE));
}
