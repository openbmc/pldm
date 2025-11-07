#include "../state_set/health_state.hpp"

#include <gtest/gtest.h>

using namespace pldm::platform_mc;
using HealthStateValue = StateSetHealthState::HealthStateValue;

TEST(HealthStateTest, GetStateSetId)
{
    EXPECT_EQ(StateSetHealthState::getHealthStateSetId(), 1);
}

TEST(HealthStateTest, ToStringValid)
{
    EXPECT_EQ(
        StateSetHealthState::healthStateToString(HealthStateValue::Normal),
        "Normal");
    EXPECT_EQ(
        StateSetHealthState::healthStateToString(HealthStateValue::NonCritical),
        "Non-Critical");
    EXPECT_EQ(
        StateSetHealthState::healthStateToString(HealthStateValue::Critical),
        "Critical");
    EXPECT_EQ(StateSetHealthState::healthStateToString(HealthStateValue::Fatal),
              "Fatal");
    EXPECT_EQ(StateSetHealthState::healthStateToString(
                  HealthStateValue::UpperNonCritical),
              "Upper Non-Critical");
    EXPECT_EQ(StateSetHealthState::healthStateToString(
                  HealthStateValue::LowerNonCritical),
              "Lower Non-Critical");
    EXPECT_EQ(StateSetHealthState::healthStateToString(
                  HealthStateValue::UpperCritical),
              "Upper Critical");
    EXPECT_EQ(StateSetHealthState::healthStateToString(
                  HealthStateValue::LowerCritical),
              "Lower Critical");
    EXPECT_EQ(
        StateSetHealthState::healthStateToString(HealthStateValue::UpperFatal),
        "Upper Fatal");
    EXPECT_EQ(
        StateSetHealthState::healthStateToString(HealthStateValue::LowerFatal),
        "Lower Fatal");
}

TEST(HealthStateTest, ToValue)
{
    EXPECT_EQ(StateSetHealthState::healthStateToValue(HealthStateValue::Normal),
              1);
    EXPECT_EQ(
        StateSetHealthState::healthStateToValue(HealthStateValue::NonCritical),
        2);
    EXPECT_EQ(
        StateSetHealthState::healthStateToValue(HealthStateValue::Critical), 3);
    EXPECT_EQ(StateSetHealthState::healthStateToValue(HealthStateValue::Fatal),
              4);
    EXPECT_EQ(StateSetHealthState::healthStateToValue(
                  HealthStateValue::UpperNonCritical),
              5);
    EXPECT_EQ(StateSetHealthState::healthStateToValue(
                  HealthStateValue::LowerNonCritical),
              6);
    EXPECT_EQ(StateSetHealthState::healthStateToValue(
                  HealthStateValue::UpperCritical),
              7);
    EXPECT_EQ(StateSetHealthState::healthStateToValue(
                  HealthStateValue::LowerCritical),
              8);
    EXPECT_EQ(
        StateSetHealthState::healthStateToValue(HealthStateValue::UpperFatal),
        9);
    EXPECT_EQ(
        StateSetHealthState::healthStateToValue(HealthStateValue::LowerFatal),
        10);
}

TEST(HealthStateTest, FromValueValid)
{
    auto state = StateSetHealthState::healthStateFromValue(1);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state.value(), HealthStateValue::Normal);

    state = StateSetHealthState::healthStateFromValue(2);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state.value(), HealthStateValue::NonCritical);

    state = StateSetHealthState::healthStateFromValue(3);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state.value(), HealthStateValue::Critical);

    state = StateSetHealthState::healthStateFromValue(4);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state.value(), HealthStateValue::Fatal);

    state = StateSetHealthState::healthStateFromValue(5);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state.value(), HealthStateValue::UpperNonCritical);

    state = StateSetHealthState::healthStateFromValue(6);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state.value(), HealthStateValue::LowerNonCritical);

    state = StateSetHealthState::healthStateFromValue(7);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state.value(), HealthStateValue::UpperCritical);

    state = StateSetHealthState::healthStateFromValue(8);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state.value(), HealthStateValue::LowerCritical);

    state = StateSetHealthState::healthStateFromValue(9);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state.value(), HealthStateValue::UpperFatal);

    state = StateSetHealthState::healthStateFromValue(10);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state.value(), HealthStateValue::LowerFatal);
}

TEST(HealthStateTest, FromValueInvalid)
{
    auto state = StateSetHealthState::healthStateFromValue(0);
    EXPECT_FALSE(state.has_value());

    state = StateSetHealthState::healthStateFromValue(11);
    EXPECT_FALSE(state.has_value());

    state = StateSetHealthState::healthStateFromValue(255);
    EXPECT_FALSE(state.has_value());
}

TEST(HealthStateTest, IsValidState)
{
    EXPECT_TRUE(StateSetHealthState::isValidHealthState(1));
    EXPECT_TRUE(StateSetHealthState::isValidHealthState(2));
    EXPECT_TRUE(StateSetHealthState::isValidHealthState(3));
    EXPECT_TRUE(StateSetHealthState::isValidHealthState(4));
    EXPECT_TRUE(StateSetHealthState::isValidHealthState(5));
    EXPECT_TRUE(StateSetHealthState::isValidHealthState(6));
    EXPECT_TRUE(StateSetHealthState::isValidHealthState(7));
    EXPECT_TRUE(StateSetHealthState::isValidHealthState(8));
    EXPECT_TRUE(StateSetHealthState::isValidHealthState(9));
    EXPECT_TRUE(StateSetHealthState::isValidHealthState(10));

    EXPECT_FALSE(StateSetHealthState::isValidHealthState(0));
    EXPECT_FALSE(StateSetHealthState::isValidHealthState(11));
    EXPECT_FALSE(StateSetHealthState::isValidHealthState(255));
}

TEST(HealthStateTest, IsFaulted)
{
    EXPECT_FALSE(
        StateSetHealthState::isHealthFaulted(HealthStateValue::Normal));
    EXPECT_TRUE(
        StateSetHealthState::isHealthFaulted(HealthStateValue::NonCritical));
    EXPECT_TRUE(
        StateSetHealthState::isHealthFaulted(HealthStateValue::Critical));
    EXPECT_TRUE(StateSetHealthState::isHealthFaulted(HealthStateValue::Fatal));
    EXPECT_TRUE(StateSetHealthState::isHealthFaulted(
        HealthStateValue::UpperNonCritical));
    EXPECT_TRUE(StateSetHealthState::isHealthFaulted(
        HealthStateValue::LowerNonCritical));
    EXPECT_TRUE(
        StateSetHealthState::isHealthFaulted(HealthStateValue::UpperCritical));
    EXPECT_TRUE(
        StateSetHealthState::isHealthFaulted(HealthStateValue::LowerCritical));
    EXPECT_TRUE(
        StateSetHealthState::isHealthFaulted(HealthStateValue::UpperFatal));
    EXPECT_TRUE(
        StateSetHealthState::isHealthFaulted(HealthStateValue::LowerFatal));
}

TEST(HealthStateTest, IsCriticalOrWorse)
{
    EXPECT_FALSE(
        StateSetHealthState::isHealthCriticalOrWorse(HealthStateValue::Normal));
    EXPECT_FALSE(StateSetHealthState::isHealthCriticalOrWorse(
        HealthStateValue::NonCritical));
    EXPECT_TRUE(StateSetHealthState::isHealthCriticalOrWorse(
        HealthStateValue::Critical));
    EXPECT_TRUE(
        StateSetHealthState::isHealthCriticalOrWorse(HealthStateValue::Fatal));
    EXPECT_FALSE(StateSetHealthState::isHealthCriticalOrWorse(
        HealthStateValue::UpperNonCritical));
    EXPECT_FALSE(StateSetHealthState::isHealthCriticalOrWorse(
        HealthStateValue::LowerNonCritical));
    EXPECT_TRUE(StateSetHealthState::isHealthCriticalOrWorse(
        HealthStateValue::UpperCritical));
    EXPECT_TRUE(StateSetHealthState::isHealthCriticalOrWorse(
        HealthStateValue::LowerCritical));
    EXPECT_TRUE(StateSetHealthState::isHealthCriticalOrWorse(
        HealthStateValue::UpperFatal));
    EXPECT_TRUE(StateSetHealthState::isHealthCriticalOrWorse(
        HealthStateValue::LowerFatal));
}

TEST(HealthStateTest, IsFatal)
{
    EXPECT_FALSE(StateSetHealthState::isHealthFatal(HealthStateValue::Normal));
    EXPECT_FALSE(
        StateSetHealthState::isHealthFatal(HealthStateValue::NonCritical));
    EXPECT_FALSE(
        StateSetHealthState::isHealthFatal(HealthStateValue::Critical));
    EXPECT_TRUE(StateSetHealthState::isHealthFatal(HealthStateValue::Fatal));
    EXPECT_FALSE(
        StateSetHealthState::isHealthFatal(HealthStateValue::UpperNonCritical));
    EXPECT_FALSE(
        StateSetHealthState::isHealthFatal(HealthStateValue::LowerNonCritical));
    EXPECT_FALSE(
        StateSetHealthState::isHealthFatal(HealthStateValue::UpperCritical));
    EXPECT_FALSE(
        StateSetHealthState::isHealthFatal(HealthStateValue::LowerCritical));
    EXPECT_TRUE(
        StateSetHealthState::isHealthFatal(HealthStateValue::UpperFatal));
    EXPECT_TRUE(
        StateSetHealthState::isHealthFatal(HealthStateValue::LowerFatal));
}

TEST(HealthStateTest, GetSeverityLevel)
{
    EXPECT_EQ(
        StateSetHealthState::getHealthSeverityLevel(HealthStateValue::Normal),
        0);
    EXPECT_EQ(StateSetHealthState::getHealthSeverityLevel(
                  HealthStateValue::NonCritical),
              1);
    EXPECT_EQ(
        StateSetHealthState::getHealthSeverityLevel(HealthStateValue::Critical),
        2);
    EXPECT_EQ(
        StateSetHealthState::getHealthSeverityLevel(HealthStateValue::Fatal),
        3);
    EXPECT_EQ(StateSetHealthState::getHealthSeverityLevel(
                  HealthStateValue::UpperNonCritical),
              1);
    EXPECT_EQ(StateSetHealthState::getHealthSeverityLevel(
                  HealthStateValue::LowerNonCritical),
              1);
    EXPECT_EQ(StateSetHealthState::getHealthSeverityLevel(
                  HealthStateValue::UpperCritical),
              2);
    EXPECT_EQ(StateSetHealthState::getHealthSeverityLevel(
                  HealthStateValue::LowerCritical),
              2);
    EXPECT_EQ(StateSetHealthState::getHealthSeverityLevel(
                  HealthStateValue::UpperFatal),
              3);
    EXPECT_EQ(StateSetHealthState::getHealthSeverityLevel(
                  HealthStateValue::LowerFatal),
              3);
}

TEST(HealthStateTest, IsMoreSevere)
{
    // Normal is less severe than everything
    EXPECT_FALSE(StateSetHealthState::isHealthMoreSevere(
        HealthStateValue::Normal, HealthStateValue::NonCritical));
    EXPECT_FALSE(StateSetHealthState::isHealthMoreSevere(
        HealthStateValue::Normal, HealthStateValue::Critical));
    EXPECT_FALSE(StateSetHealthState::isHealthMoreSevere(
        HealthStateValue::Normal, HealthStateValue::Fatal));

    // NonCritical is more severe than Normal
    EXPECT_TRUE(StateSetHealthState::isHealthMoreSevere(
        HealthStateValue::NonCritical, HealthStateValue::Normal));
    EXPECT_FALSE(StateSetHealthState::isHealthMoreSevere(
        HealthStateValue::NonCritical, HealthStateValue::Critical));
    EXPECT_FALSE(StateSetHealthState::isHealthMoreSevere(
        HealthStateValue::NonCritical, HealthStateValue::Fatal));

    // Critical is more severe than Normal and NonCritical
    EXPECT_TRUE(StateSetHealthState::isHealthMoreSevere(
        HealthStateValue::Critical, HealthStateValue::Normal));
    EXPECT_TRUE(StateSetHealthState::isHealthMoreSevere(
        HealthStateValue::Critical, HealthStateValue::NonCritical));
    EXPECT_FALSE(StateSetHealthState::isHealthMoreSevere(
        HealthStateValue::Critical, HealthStateValue::Fatal));

    // Fatal is most severe
    EXPECT_TRUE(StateSetHealthState::isHealthMoreSevere(
        HealthStateValue::Fatal, HealthStateValue::Normal));
    EXPECT_TRUE(StateSetHealthState::isHealthMoreSevere(
        HealthStateValue::Fatal, HealthStateValue::NonCritical));
    EXPECT_TRUE(StateSetHealthState::isHealthMoreSevere(
        HealthStateValue::Fatal, HealthStateValue::Critical));

    // Upper/Lower variants have same severity as their base
    EXPECT_FALSE(StateSetHealthState::isHealthMoreSevere(
        HealthStateValue::UpperNonCritical, HealthStateValue::NonCritical));
    EXPECT_FALSE(StateSetHealthState::isHealthMoreSevere(
        HealthStateValue::LowerCritical, HealthStateValue::Critical));
    EXPECT_FALSE(StateSetHealthState::isHealthMoreSevere(
        HealthStateValue::UpperFatal, HealthStateValue::Fatal));
}

TEST(HealthStateTest, GetAllStates)
{
    auto allStates = StateSetHealthState::getAllHealthStates();
    EXPECT_EQ(allStates.size(), 10);

    // Verify all states are present
    EXPECT_NE(std::find(allStates.begin(), allStates.end(),
                        HealthStateValue::Normal),
              allStates.end());
    EXPECT_NE(std::find(allStates.begin(), allStates.end(),
                        HealthStateValue::NonCritical),
              allStates.end());
    EXPECT_NE(std::find(allStates.begin(), allStates.end(),
                        HealthStateValue::Critical),
              allStates.end());
    EXPECT_NE(std::find(allStates.begin(), allStates.end(),
                        HealthStateValue::Fatal),
              allStates.end());
    EXPECT_NE(std::find(allStates.begin(), allStates.end(),
                        HealthStateValue::UpperNonCritical),
              allStates.end());
    EXPECT_NE(std::find(allStates.begin(), allStates.end(),
                        HealthStateValue::LowerNonCritical),
              allStates.end());
    EXPECT_NE(std::find(allStates.begin(), allStates.end(),
                        HealthStateValue::UpperCritical),
              allStates.end());
    EXPECT_NE(std::find(allStates.begin(), allStates.end(),
                        HealthStateValue::LowerCritical),
              allStates.end());
    EXPECT_NE(std::find(allStates.begin(), allStates.end(),
                        HealthStateValue::UpperFatal),
              allStates.end());
    EXPECT_NE(std::find(allStates.begin(), allStates.end(),
                        HealthStateValue::LowerFatal),
              allStates.end());
}
