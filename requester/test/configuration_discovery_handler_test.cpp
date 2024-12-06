#include "config.h"

#include "common/test/mocked_utils.hpp"
#include "common/types.hpp"
#include "common/utils.hpp"
#include "requester/configuration_discovery_handler.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;

TEST(ConfigurationDiscoveryHandlerTest, succesfullyDiscoverAndRemoveConfig)
{
    constexpr uint8_t EID = 10;
    constexpr auto mockedDbusPath =
        "/xyz/openbmc_project/inventory/system/board/Mocked_Board_Slot_1/MockedDevice";
    constexpr auto mockedService = "xyz.openbmc_project.EntityManager";
    std::vector<std::string> mockedInterfaces{
        "xyz.openbmc_project.Configuration.MCTPI2CTarget",
        "xyz.openbmc_project.Configuration.MCTPI3CTarget"};
    const sdbusplus::message::object_path mockedMctpReactorObjectPath{
        "/au/com/codeconstruct/mctp1/networks/1/endpoints/10/configured_by"};
    const sdbusplus::message::object_path mockedSubTreePath{
        "/xyz/openbmc_project/inventory/system"};

    MockdBusHandler mockedUtils;
    auto handler =
        std::make_unique<pldm::ConfigurationDiscoveryHandler>(&mockedUtils);

    pldm::utils::GetAssociatedSubTreeResponse
        mockedGetAssociatedSubTreeResponse{
            {mockedDbusPath, {{mockedService, mockedInterfaces}}}};

    EXPECT_CALL(mockedUtils,
                getAssociatedSubTree(mockedMctpReactorObjectPath,
                                     mockedSubTreePath, 3, mockedInterfaces))
        .Times(1)
        .WillOnce(testing::Return(mockedGetAssociatedSubTreeResponse));

    pldm::utils::PropertyMap mockGetI2CTargetPropertiesResponse{
        {"Address", uint64_t(0x1)}, {"Bus", uint64_t(0)}};

    EXPECT_CALL(mockedUtils, getDbusPropertiesVariant(_, _, _))
        .Times(1)
        .WillOnce(testing::Return(mockGetI2CTargetPropertiesResponse));

    pldm::MctpInfos mctpInfos;
    mctpInfos.emplace_back(pldm::MctpInfo(EID, "emptyUUID", "", 1));

    // Act
    handler->handleMctpEndpoints(mctpInfos);

    ASSERT_EQ(1, handler->getConfigurations().size());

    // Act
    handler->handleRemovedMctpEndpoints(mctpInfos);

    ASSERT_EQ(0, handler->getConfigurations().size());
}

TEST(ConfigurationDiscoveryHandlerTest, BlockedByNoSuchElement)
{
    constexpr uint8_t EID = 10;
    constexpr auto mockedDbusPath =
        "/xyz/openbmc_project/inventory/system/board/Mocked_Board_Slot_1/MockedDevice";
    constexpr auto mockedService = "xyz.openbmc_project.EntityManager";
    std::vector<std::string> mockedInterfaces{
        "xyz.openbmc_project.Configuration.MCTPI2CTarget",
        "xyz.openbmc_project.Configuration.MCTPI3CTarget"};
    const sdbusplus::message::object_path mockedMctpReactorObjectPath{
        "/au/com/codeconstruct/mctp1/networks/1/endpoints/10/configured_by"};
    const sdbusplus::message::object_path mockedSubTreePath{
        "/xyz/openbmc_project/inventory/system"};

    MockdBusHandler mockedUtils;
    auto handler =
        std::make_unique<pldm::ConfigurationDiscoveryHandler>(&mockedUtils);

    pldm::utils::GetAssociatedSubTreeResponse
        mockedGetAssociatedSubTreeResponse{
            {mockedDbusPath, {{mockedService, mockedInterfaces}}}};

    EXPECT_CALL(mockedUtils,
                getAssociatedSubTree(mockedMctpReactorObjectPath,
                                     mockedSubTreePath, 3, mockedInterfaces))
        .Times(1)
        .WillOnce(testing::Return(mockedGetAssociatedSubTreeResponse));

    pldm::utils::PropertyMap mockGetI2CTargetPropertiesResponse{
        {"Address", uint64_t(0x1)}};

    EXPECT_CALL(mockedUtils, getDbusPropertiesVariant(_, _, _))
        .Times(1)
        .WillOnce(testing::Return(mockGetI2CTargetPropertiesResponse));

    pldm::MctpInfos mctpInfos;
    mctpInfos.emplace_back(pldm::MctpInfo(EID, "emptyUUID", "", 1));

    // Act
    EXPECT_NO_THROW(handler->handleMctpEndpoints(mctpInfos));

    ASSERT_EQ(0, handler->getConfigurations().size());
}
