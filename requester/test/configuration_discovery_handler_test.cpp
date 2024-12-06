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
    constexpr auto mockedInterface =
        "xyz.openbmc_project.Configuration.PLDMDevice";
    const sdbusplus::message::object_path mockedMctpReactorObjectPath{
        "/au/com/codeconstruct/mctp1/networks/1/endpoints/10/configured_by"};

    MockdBusHandler mockedUtils;
    auto handler =
        std::make_unique<pldm::ConfigurationDiscoveryHandler>(&mockedUtils);

    pldm::utils::GetAssociatedSubTreeResponse
        mockedGetAssociatedSubTreeResponse{
            {mockedDbusPath, {{mockedService, {mockedInterface}}}}};

    EXPECT_CALL(mockedUtils, getAssociatedSubTree(mockedMctpReactorObjectPath,
                                                  std::string{mockedInterface}))
        .Times(1)
        .WillOnce(testing::Return(mockedGetAssociatedSubTreeResponse));

    pldm::utils::PropertyMap mockGetDbusPropertiesResponse{
        {"Name", std::string("MockedDevice")},
        {"RanOver", std::string("MCTPI2CTarget")}};

    pldm::utils::PropertyMap mockGetI2CTargetPropertiesResponse{
        {"Address", uint64_t(0x1)}, {"Bus", uint64_t(0)}};

    pldm::utils::PropertyMap mockGetFwInfoPropertiesResponse{
        {"VendorIANA", uint32_t(0x1)},
        {"CompatibleHardware", std::string("MockedHardware")}};

    EXPECT_CALL(mockedUtils, getDbusPropertiesVariant(_, _, _))
        .Times(3)
        .WillOnce(testing::Return(mockGetDbusPropertiesResponse))
        .WillOnce(testing::Return(mockGetI2CTargetPropertiesResponse))
        .WillOnce(testing::Return(mockGetFwInfoPropertiesResponse));

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
    constexpr auto mockedInterface =
        "xyz.openbmc_project.Configuration.PLDMDevice";
    [[maybe_unused]] constexpr auto mockedFwInterface =
        "xyz.openbmc_project.Configuration.PLDMDevice.FirmwareInfo";
    const sdbusplus::message::object_path mockedMctpReactorObjectPath{
        "/au/com/codeconstruct/mctp1/networks/1/endpoints/10/configured_by"};
    MockdBusHandler mockedUtils;
    auto handler =
        std::make_unique<pldm::ConfigurationDiscoveryHandler>(&mockedUtils);

    pldm::utils::GetAssociatedSubTreeResponse
        mockedGetAssociatedSubTreeResponse{
            {mockedDbusPath, {{mockedService, {mockedInterface}}}}};

    EXPECT_CALL(mockedUtils, getAssociatedSubTree(_, _))
        .Times(1)
        .WillOnce(testing::Return(mockedGetAssociatedSubTreeResponse));

    pldm::utils::PropertyMap mockGetDbusPropertiesResponse{
        {"Name", std::string("MockedDevice")}};

    EXPECT_CALL(mockedUtils, getDbusPropertiesVariant(_, _, _))
        .Times(1)
        .WillOnce(testing::Return(mockGetDbusPropertiesResponse));

    pldm::MctpInfos mctpInfos;
    mctpInfos.emplace_back(pldm::MctpInfo(EID, "emptyUUID", "", 1));

    // Act
    EXPECT_NO_THROW(handler->handleMctpEndpoints(mctpInfos));

    ASSERT_EQ(0, handler->getConfigurations().size());
}
