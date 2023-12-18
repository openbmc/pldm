#include "config.h"

#include "common/test/mocked_utils.hpp"
#include "common/types.hpp"
#include "common/utils.hpp"
#include "requester/configuration_discovery_handler.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;

TEST(ConfigurationDiscoveryHandlerTest, succesfullyDiscoverConfig)
{
    constexpr uint8_t EID = 10;
    constexpr auto mockedDbusPath =
        "/xyz/openbmc_project/inventory/system/board/Mocked_Board_Slot_1/MockedDevice";
    constexpr auto mockedService = "xyz.openbmc_project.EntityManager";
    constexpr auto mockedInterface =
        "xyz.openbmc_project.Configuration.MCTPEndpoint";
    MockdBusHandler mockedUtils;
    auto handler =
        std::make_unique<pldm::ConfigurationDiscoveryHandler>(&mockedUtils);

    pldm::utils::GetSubTreeResponse mockedGetSubtreeResponse{std::make_pair(
        mockedDbusPath,
        pldm::utils::MapperServiceMap{std::make_pair(
            mockedService, pldm::utils::Interfaces{mockedInterface})})};

    EXPECT_CALL(mockedUtils, getSubtree(_, _, _))
        .Times(1)
        .WillOnce(testing::Return(mockedGetSubtreeResponse));

    pldm::utils::PropertyMap mockGetAllResponse{
        {"Address", uint64_t(0x1)},
        {"Bus", uint64_t(0)},
        {"EndpointId", uint64_t(EID)},
        {"Name", std::string("MockedDevice")}};

    EXPECT_CALL(mockedUtils,
                getAll(mockedService, mockedDbusPath, mockedInterface))
        .Times(1)
        .WillOnce(testing::Return(mockGetAllResponse));

    pldm::MctpInfos mctpInfos;
    mctpInfos.emplace_back(pldm::MctpInfo(EID, "emptyUUID", "", 1));

    // Act
    handler->handleMctpEndpoints(mctpInfos);

    ASSERT_EQ(1, handler->getConfigurations().size());
}

TEST(ConfigurationDiscoveryHandlerTest, BlockedByNoSuchElement)
{
    constexpr uint8_t EID = 10;
    constexpr auto mockedDbusPath =
        "/xyz/openbmc_project/inventory/system/board/Mocked_Board_Slot_1/MockedDevice";
    constexpr auto mockedService = "xyz.openbmc_project.EntityManager";
    constexpr auto mockedInterface =
        "xyz.openbmc_project.Configuration.MCTPEndpoint";
    MockdBusHandler mockedUtils;
    auto handler =
        std::make_unique<pldm::ConfigurationDiscoveryHandler>(&mockedUtils);

    pldm::utils::GetSubTreeResponse mockedGetSubtreeResponse{std::make_pair(
        mockedDbusPath,
        pldm::utils::MapperServiceMap{std::make_pair(
            mockedService, pldm::utils::Interfaces{mockedInterface})})};

    EXPECT_CALL(mockedUtils, getSubtree(_, _, _))
        .Times(1)
        .WillOnce(testing::Return(mockedGetSubtreeResponse));

    pldm::utils::PropertyMap mockGetAllResponse{
        {"Address", uint64_t(0x1)},
        {"Bus", uint64_t(0)},
        /* No EndpointId */
        {"Name", std::string("MockedDevice")}};

    EXPECT_CALL(mockedUtils,
                getAll(mockedService, mockedDbusPath, mockedInterface))
        .Times(1)
        .WillOnce(testing::Return(mockGetAllResponse));

    pldm::MctpInfos mctpInfos;
    mctpInfos.emplace_back(pldm::MctpInfo(EID, "emptyUUID", "", 1));

    // Act
    EXPECT_NO_THROW(handler->handleMctpEndpoints(mctpInfos));

    ASSERT_EQ(0, handler->getConfigurations().size());
}

TEST(ConfigurationDiscoveryHandlerTest, succesfullyRemoveConfig)
{
    constexpr uint8_t EID = 10;

    MockdBusHandler mockedUtils;
    auto handler =
        std::make_unique<pldm::ConfigurationDiscoveryHandler>(&mockedUtils);

    pldm::MctpEndpoint mockedMctpEndpoint = {uint64_t(0x1), uint64_t(EID),
                                             uint64_t(0), "MockedDevice"};
    handler->getConfigurations().emplace(
        "/xyz/openbmc_project/inventory/system/board/Mocked_Board_Slot_1/MockedDevice",
        mockedMctpEndpoint);

    pldm::MctpInfos mctpInfos;
    mctpInfos.emplace_back(pldm::MctpInfo(EID, "emptyUUID", "", 1));
    handler->handleRemovedMctpEndpoints(mctpInfos);

    ASSERT_EQ(0, handler->getConfigurations().size());
}
