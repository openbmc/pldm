#include "../activation_methods.hpp"

#include <gtest/gtest.h>

using pldm::fw_update::ComponentActivationMethods;
using Methods = ComponentActivationMethods;

TEST(ActivationMethods, NoAppliedComponents)
{
    Methods methods;
    EXPECT_FALSE(methods.requestSelfContained());
    EXPECT_TRUE(methods.pendingMethods(false).empty());
}

TEST(ActivationMethods, SelfContainedAlternativeAvoidsAcCycle)
{
    Methods methods;
    methods.recordApplied(0, 0x22, 0x22, PLDM_FWUP_APPLY_SUCCESS, 0);
    EXPECT_TRUE(methods.requestSelfContained());
    EXPECT_TRUE(methods.pendingMethods(true).empty());
    EXPECT_EQ(methods.pendingMethods(false), "Component 0: ACPowerCycle");
}

TEST(ActivationMethods, IndependentAcRequirementIsPreserved)
{
    Methods methods;
    methods.recordApplied(0, 0x22, 0x22, PLDM_FWUP_APPLY_SUCCESS, 0);
    methods.recordApplied(1, 0x20, 0x20, PLDM_FWUP_APPLY_SUCCESS, 0);
    EXPECT_TRUE(methods.requestSelfContained());
    EXPECT_EQ(methods.pendingMethods(true), "Component 1: ACPowerCycle");
}

TEST(ActivationMethods, DeviceWideRequestRespectsPackageRestrictions)
{
    Methods methods;
    methods.recordApplied(0, 0x22, 0x22, PLDM_FWUP_APPLY_SUCCESS, 0);
    methods.recordApplied(1, 0x20, 0x22, PLDM_FWUP_APPLY_SUCCESS, 0);
    EXPECT_FALSE(methods.requestSelfContained());
    EXPECT_EQ(methods.pendingMethods(false),
              "Component 0: ACPowerCycle; Component 1: ACPowerCycle");
}

TEST(ActivationMethods, AutomaticNeedsNoAction)
{
    Methods methods;
    methods.recordApplied(0, 0x21, 0x21, PLDM_FWUP_APPLY_SUCCESS, 0);
    EXPECT_FALSE(methods.requestSelfContained());
    EXPECT_TRUE(methods.pendingMethods(false).empty());
}

TEST(ActivationMethods, ModificationReplacesCapabilitiesNotPackageRequest)
{
    Methods methods;
    methods.recordApplied(0, 0x22, 0x22,
                          PLDM_FWUP_APPLY_SUCCESS_WITH_ACTIVATION_METHOD, 0x20);
    EXPECT_FALSE(methods.requestSelfContained());
    EXPECT_EQ(methods.pendingMethods(false), "Component 0: ACPowerCycle");
    methods.recordApplied(0, 0x20, 0x20,
                          PLDM_FWUP_APPLY_SUCCESS_WITH_ACTIVATION_METHOD, 0x02);
    EXPECT_FALSE(methods.requestSelfContained());
    EXPECT_EQ(methods.pendingMethods(false), "Component 0: UnknownActivationMethod");
}

TEST(ActivationMethods, ModificationCanEnableSelfContained)
{
    Methods methods;
    methods.recordApplied(0, 0x22, 0x20,
                          PLDM_FWUP_APPLY_SUCCESS_WITH_ACTIVATION_METHOD, 0x02);
    EXPECT_TRUE(methods.requestSelfContained());
    EXPECT_TRUE(methods.pendingMethods(true).empty());
}

TEST(ActivationMethods, OrdinarySuccessIgnoresModification)
{
    Methods methods;
    methods.recordApplied(0, 0x22, 0x20, PLDM_FWUP_APPLY_SUCCESS, 0x02);
    EXPECT_FALSE(methods.requestSelfContained());
    EXPECT_EQ(methods.pendingMethods(false), "Component 0: ACPowerCycle");
}

TEST(ActivationMethods, FailedAndSkippedComponentsDoNotContribute)
{
    Methods methods;
    methods.recordApplied(0, 0x20, 0x20, 0x02, 0x20);
    methods.recordApplied(1, 0x02, 0x02, PLDM_FWUP_APPLY_SUCCESS, 0);
    EXPECT_TRUE(methods.pendingMethods(true).empty());
}

TEST(ActivationMethods, IndependentManualActionsAreNotCollapsed)
{
    Methods methods;
    methods.recordApplied(4, 0x04, 0x04, PLDM_FWUP_APPLY_SUCCESS, 0);
    methods.recordApplied(7, 0x20, 0x20, PLDM_FWUP_APPLY_SUCCESS, 0);
    EXPECT_EQ(methods.pendingMethods(false),
              "Component 4: MediumSpecificReset; Component 7: ACPowerCycle");
}

TEST(ActivationMethods, SelectsOnlySupportedManualAlternatives)
{
    Methods methods;
    methods.recordApplied(0, 0x3c, 0x30, PLDM_FWUP_APPLY_SUCCESS, 0);
    EXPECT_EQ(methods.pendingMethods(false), "Component 0: DCPowerCycle");
}

TEST(ActivationMethods, ZeroAndDifferentlyDefinedBitsDoNotImplyActive)
{
    for (uint16_t mask : {0x0000, 0x0040, 0x0080, 0x8000})
    {
        Methods methods;
        methods.recordApplied(0, mask, mask, PLDM_FWUP_APPLY_SUCCESS, 0);
        EXPECT_FALSE(methods.requestSelfContained());
        EXPECT_EQ(methods.pendingMethods(false), "Component 0: UnknownActivationMethod");
    }
}