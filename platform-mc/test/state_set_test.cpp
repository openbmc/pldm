#include "platform-mc/state_set.hpp"
#include "platform-mc/state_set_performance.hpp"

#include <libpldm/state_set.h>

#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace pldm::platform_mc;

using OperationalStatusIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::State::
                                    Decorator::server::OperationalStatus>;

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
    StateSets stateSets("/xyz/openbmc_project/inventory/test/state_set");

    /* The performance state set has a D-Bus interface */
    EXPECT_NE(nullptr, createStateSet(stateSets, PLDM_STATE_SET_PERFORMANCE));

    /* A state set whose interface is not added yet has none */
    EXPECT_EQ(nullptr, createStateSet(stateSets, PLDM_STATE_SET_HEALTH_STATE));
    EXPECT_EQ(nullptr, createStateSet(stateSets, PLDM_STATE_SET_PRESENCE));
    EXPECT_EQ(nullptr,
              createStateSet(stateSets, PLDM_STATE_SET_CONFIGURATION_STATE));
}

TEST(StateSetTest, performanceStatusTest)
{
    StateSets stateSets("/xyz/openbmc_project/inventory/test/performance");
    StateSetPerformance stateSet(stateSets);

    /* A throttled entity and a degraded entity are two states, so each is
     * carried by its own property and the other one stays clear
     */
    stateSet.setPresentState(PLDM_STATE_SET_PERFORMANCE_THROTTLED);
    EXPECT_TRUE(stateSet.throttled());
    EXPECT_FALSE(stateSet.degraded());

    stateSet.setPresentState(PLDM_STATE_SET_PERFORMANCE_DEGRADED);
    EXPECT_TRUE(stateSet.degraded());
    EXPECT_FALSE(stateSet.throttled());

    /* Each reading replaces the previous one, so the entity comes back to
     * normal when the terminus reports it normal again
     */
    stateSet.setPresentState(PLDM_STATE_SET_PERFORMANCE_NORMAL);
    EXPECT_FALSE(stateSet.throttled());
    EXPECT_FALSE(stateSet.degraded());
}

TEST(StateSetTest, performanceThrottleCausesTest)
{
    StateSets stateSets(
        "/xyz/openbmc_project/inventory/test/performance_causes");
    StateSetPerformance stateSet(stateSets);
    std::vector<ThrottleReasons> unknownCause{ThrottleReasons::Unknown};

    /* The state set reports no cause of the throttling, so the entity is
     * throttled for an unknown cause
     */
    stateSet.setPresentState(PLDM_STATE_SET_PERFORMANCE_THROTTLED);
    EXPECT_EQ(unknownCause, stateSet.throttleCauses());

    /* An entity which is not throttled has no cause */
    stateSet.setPresentState(PLDM_STATE_SET_PERFORMANCE_NORMAL);
    EXPECT_TRUE(stateSet.throttleCauses().empty());

    stateSet.setPresentState(PLDM_STATE_SET_PERFORMANCE_THROTTLED);
    stateSet.setPresentState(PLDM_STATE_SET_PERFORMANCE_DEGRADED);
    EXPECT_TRUE(stateSet.throttleCauses().empty());
}

TEST(StateSetTest, performanceUnknownStateTest)
{
    StateSets stateSets(
        "/xyz/openbmc_project/inventory/test/performance_unknown");
    StateSetPerformance stateSet(stateSets);

    /* A state the state set does not define keeps the performance of the last
     * reading a state value was defined for
     */
    stateSet.setPresentState(PLDM_STATE_SET_PERFORMANCE_THROTTLED);
    stateSet.setPresentState(0xff);
    EXPECT_TRUE(stateSet.throttled());

    stateSet.setPresentState(PLDM_STATE_SET_PERFORMANCE_DEGRADED);
    stateSet.setPresentState(0xff);
    EXPECT_TRUE(stateSet.degraded());
    EXPECT_FALSE(stateSet.throttled());
}

TEST(StateSetTest, performanceDegradationTest)
{
    StateSets stateSets(
        "/xyz/openbmc_project/inventory/test/performance_degradation");
    StateSetPerformance stateSet(stateSets);
    auto& degradation = stateSets.getInterface<DegradationIntf>();

    /* A degraded entity is carried by the Degradation interface which the
     * state sets of the entity share
     */
    EXPECT_FALSE(degradation.degraded());

    stateSet.setPresentState(PLDM_STATE_SET_PERFORMANCE_DEGRADED);
    EXPECT_TRUE(degradation.degraded());
}
