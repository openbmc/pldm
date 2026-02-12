#include "common/test/mocked_utils.hpp"
#include "common/types.hpp"
#include "common/utils.hpp"
#include "requester/test/mock_mctp_discovery_handler_intf.hpp"

#include <systemd/sd-bus.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;

class TestMctpDiscovery : public ::testing::Test
{
  public:
    static const pldm::Configurations& getConfigurations(
        const pldm::MctpDiscovery& mctpDiscovery)
    {
        return mctpDiscovery.configurations;
    }
    static void searchConfigurationFor(pldm::MctpDiscovery& mctpDiscovery,
                                       pldm::utils::DBusHandler& handler,
                                       pldm::MctpInfo& mctpInfo)
    {
        mctpDiscovery.searchConfigurationFor(handler, mctpInfo);
    }
    static auto parseMctpEndpointPath(const std::string& path)
    {
        return pldm::MctpDiscovery::parseMctpEndpointPath(path);
    }
};

TEST(MctpEndpointDiscoveryTest, SingleHandleMctpEndpoint)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    pldm::MockManager manager;

    EXPECT_CALL(manager, handleMctpEndpoints(_)).Times(1);

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager});
    mctpDiscoveryHandler = nullptr;
}

TEST(MctpEndpointDiscoveryTest, MultipleHandleMctpEndpoints)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    pldm::MockManager manager1;
    pldm::MockManager manager2;

    EXPECT_CALL(manager1, handleMctpEndpoints(_)).Times(1);
    EXPECT_CALL(manager2, handleMctpEndpoints(_)).Times(1);

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{
                 &manager1, &manager2});
    mctpDiscoveryHandler = nullptr;
}

TEST(MctpEndpointDiscoveryTest, goodGetMctpInfos)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    pldm::MockManager manager;
    std::map<pldm::MctpInfo, pldm::Availability> currentMctpInfoMap;

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager});
    mctpDiscoveryHandler->getMctpInfos(currentMctpInfoMap);
    EXPECT_EQ(currentMctpInfoMap.size(), 0);
}

TEST(MctpEndpointDiscoveryTest, goodAddToExistingMctpInfos)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    pldm::MockManager manager;
    const pldm::MctpInfos& mctpInfos = {
        pldm::MctpInfo(11, pldm::emptyUUID, "", 1, std::nullopt),
        pldm::MctpInfo(12, pldm::emptyUUID, "abc", 1, std::nullopt)};

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager});
    mctpDiscoveryHandler->addToExistingMctpInfos(mctpInfos);
    EXPECT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 2);
    pldm::MctpInfo mctpInfo = mctpDiscoveryHandler->existingMctpInfos.back();
    EXPECT_EQ(std::get<0>(mctpInfo), 12);
    EXPECT_EQ(std::get<2>(mctpInfo), "abc");
    EXPECT_EQ(std::get<3>(mctpInfo), 1);
}

TEST(MctpEndpointDiscoveryTest, badAddToExistingMctpInfos)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    pldm::MockManager manager;
    const pldm::MctpInfos& mctpInfos = {
        pldm::MctpInfo(11, pldm::emptyUUID, "", 1, std::nullopt)};

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager});
    mctpDiscoveryHandler->addToExistingMctpInfos(mctpInfos);
    EXPECT_NE(mctpDiscoveryHandler->existingMctpInfos.size(), 2);
}

TEST(MctpEndpointDiscoveryTest, parseMctpEndpointPathGood)
{
    // Standard codeconstruct path
    auto result = TestMctpDiscovery::parseMctpEndpointPath(
        "/au/com/codeconstruct/mctp1/networks/1/endpoints/10");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, 1);
    EXPECT_EQ(result->second, 10);

    // Different networkId and EID
    result = TestMctpDiscovery::parseMctpEndpointPath(
        "/au/com/codeconstruct/mctp1/networks/3/endpoints/25");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, 3);
    EXPECT_EQ(result->second, 25);

    // NetworkId 0
    result = TestMctpDiscovery::parseMctpEndpointPath(
        "/au/com/codeconstruct/mctp1/networks/0/endpoints/1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, 0);
    EXPECT_EQ(result->second, 1);
}

TEST(MctpEndpointDiscoveryTest, parseMctpEndpointPathBad)
{
    // Empty path
    EXPECT_FALSE(TestMctpDiscovery::parseMctpEndpointPath("").has_value());

    // Missing /networks/ segment
    EXPECT_FALSE(TestMctpDiscovery::parseMctpEndpointPath(
                     "/au/com/codeconstruct/mctp1/endpoints/10")
                     .has_value());

    // Missing /endpoints/ segment
    EXPECT_FALSE(TestMctpDiscovery::parseMctpEndpointPath(
                     "/au/com/codeconstruct/mctp1/networks/1")
                     .has_value());

    // Reversed order: endpoints before networks
    EXPECT_FALSE(
        TestMctpDiscovery::parseMctpEndpointPath("/endpoints/10/networks/1")
            .has_value());

    // Trailing slash after EID
    EXPECT_FALSE(TestMctpDiscovery::parseMctpEndpointPath(
                     "/au/com/codeconstruct/mctp1/networks/1/endpoints/10/")
                     .has_value());

    // Extra path segment after EID
    EXPECT_FALSE(
        TestMctpDiscovery::parseMctpEndpointPath(
            "/au/com/codeconstruct/mctp1/networks/1/endpoints/10/extra")
            .has_value());

    // Non-numeric networkId
    EXPECT_FALSE(TestMctpDiscovery::parseMctpEndpointPath(
                     "/au/com/codeconstruct/mctp1/networks/abc/endpoints/10")
                     .has_value());

    // Non-numeric EID
    EXPECT_FALSE(TestMctpDiscovery::parseMctpEndpointPath(
                     "/au/com/codeconstruct/mctp1/networks/1/endpoints/xyz")
                     .has_value());
}

TEST(MctpEndpointDiscoveryTest, goodSearchConfigurationFor)
{
    MockdBusHandler mockedDbusHandler;
    auto& bus = mockedDbusHandler.getBus();
    pldm::MockManager manager;
    const pldm::MctpInfos& mctpInfos = {
        pldm::MctpInfo(10, pldm::emptyUUID, "abc", 1, std::nullopt)};

    constexpr auto mockedDbusPath =
        "/xyz/openbmc_project/inventory/system/board/Mocked_Board_Slot_1/MockedDevice";
    constexpr auto mockedService = "xyz.openbmc_project.EntityManager";
    std::vector<std::string> mockedInterfaces{
        "xyz.openbmc_project.Configuration.MCTPI2CTarget",
        "xyz.openbmc_project.Configuration.MCTPI3CTarget"};

    pldm::utils::GetAssociatedSubTreeResponse
        mockedGetAssociatedSubTreeResponse{
            {mockedDbusPath, {{mockedService, mockedInterfaces}}}};

    EXPECT_CALL(mockedDbusHandler, getAssociatedSubTree(_, _, _, _))
        .WillOnce(testing::Return(mockedGetAssociatedSubTreeResponse));

    pldm::utils::PropertyMap mockGetI2CTargetPropertiesResponse{
        {"Address", uint64_t(0x1)},
        {"Bus", uint64_t(0)},
        {"Name", std::string("MockedDevice")}};

    EXPECT_CALL(mockedDbusHandler, getDbusPropertiesVariant(_, _, _))
        .WillOnce(testing::Return(mockGetI2CTargetPropertiesResponse));

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager});
    mctpDiscoveryHandler->addToExistingMctpInfos(mctpInfos);
    EXPECT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 1);
    pldm::MctpInfo mctpInfo = mctpDiscoveryHandler->existingMctpInfos.back();
    EXPECT_EQ(std::get<0>(mctpInfo), 10);
    EXPECT_EQ(std::get<2>(mctpInfo), "abc");
    EXPECT_EQ(std::get<3>(mctpInfo), 1);
    TestMctpDiscovery::searchConfigurationFor(*mctpDiscoveryHandler,
                                              mockedDbusHandler, mctpInfo);
    EXPECT_EQ(std::get<4>(mctpInfo),
              std::optional<std::string>("MockedDevice"));
    auto configuration =
        TestMctpDiscovery::getConfigurations(*mctpDiscoveryHandler);
    EXPECT_EQ(configuration.size(), 1);
}

TEST(MctpEndpointDiscoveryTest, badSearchConfigurationFor)
{
    MockdBusHandler mockedDbusHandler;
    auto& bus = mockedDbusHandler.getBus();
    pldm::MockManager manager;
    const pldm::MctpInfos& mctpInfos = {
        pldm::MctpInfo(10, pldm::emptyUUID, "abc", 1, std::nullopt)};

    constexpr auto mockedDbusPath =
        "/xyz/openbmc_project/inventory/system/board/Mocked_Board_Slot_1/MockedDevice";
    constexpr auto mockedService = "xyz.openbmc_project.EntityManager";
    std::vector<std::string> mockedInterfaces{
        "xyz.openbmc_project.Configuration.MCTPPCIETarget",
        "xyz.openbmc_project.Configuration.MCTPUSBTarget"};

    pldm::utils::GetAssociatedSubTreeResponse
        mockedGetAssociatedSubTreeResponse{
            {mockedDbusPath, {{mockedService, mockedInterfaces}}}};

    EXPECT_CALL(mockedDbusHandler, getAssociatedSubTree(_, _, _, _))
        .WillOnce(testing::Return(mockedGetAssociatedSubTreeResponse));

    pldm::utils::PropertyMap mockGetI2CTargetPropertiesResponse{
        {"Address", uint64_t(0x1)}, {"Bus", uint64_t(0)}};

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager});
    mctpDiscoveryHandler->addToExistingMctpInfos(mctpInfos);
    EXPECT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 1);
    pldm::MctpInfo mctpInfo = mctpDiscoveryHandler->existingMctpInfos.back();
    EXPECT_EQ(std::get<0>(mctpInfo), 10);
    EXPECT_EQ(std::get<2>(mctpInfo), "abc");
    EXPECT_EQ(std::get<3>(mctpInfo), 1);
    TestMctpDiscovery::searchConfigurationFor(*mctpDiscoveryHandler,
                                              mockedDbusHandler, mctpInfo);
    auto configuration =
        TestMctpDiscovery::getConfigurations(*mctpDiscoveryHandler);
    EXPECT_EQ(configuration.size(), 0);
}

TEST(MctpEndpointDiscoveryTest, removeEndpointsMatchFound)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    pldm::MockManager manager;

    EXPECT_CALL(manager, handleRemovedMctpEndpoints(_)).Times(1);

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager});

    // Pre-populate existingMctpInfos with an endpoint on network 1, EID 10
    const pldm::MctpInfos& seedInfos = {
        pldm::MctpInfo(10, pldm::emptyUUID, "", 1, std::nullopt)};
    mctpDiscoveryHandler->addToExistingMctpInfos(seedInfos);
    ASSERT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 1);

    // Build InterfacesRemoved signal matching that endpoint
    auto msg = bus.new_signal("/test", "org.freedesktop.DBus.ObjectManager",
                              "InterfacesRemoved");
    sdbusplus::message::object_path objPath{
        "/au/com/codeconstruct/mctp1/networks/1/endpoints/10"};
    std::vector<std::string> interfaces{MCTPEndpoint::interface};
    msg.append(objPath, interfaces);
    ASSERT_EQ(0, sd_bus_message_seal(msg.get(), 0, 0));

    mctpDiscoveryHandler->removeEndpoints(msg);
    EXPECT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 0);
}

TEST(MctpEndpointDiscoveryTest, removeEndpointsNoMatch)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    pldm::MockManager manager;

    // Handler should NOT be called when endpoint is not in existingMctpInfos
    EXPECT_CALL(manager, handleRemovedMctpEndpoints(_)).Times(0);

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager});

    // Seed with EID 10 on network 1
    const pldm::MctpInfos& seedInfos = {
        pldm::MctpInfo(10, pldm::emptyUUID, "", 1, std::nullopt)};
    mctpDiscoveryHandler->addToExistingMctpInfos(seedInfos);
    ASSERT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 1);

    // Signal removes EID 99 which doesn't exist
    auto msg = bus.new_signal("/test", "org.freedesktop.DBus.ObjectManager",
                              "InterfacesRemoved");
    sdbusplus::message::object_path objPath{
        "/au/com/codeconstruct/mctp1/networks/1/endpoints/99"};
    std::vector<std::string> interfaces{MCTPEndpoint::interface};
    msg.append(objPath, interfaces);
    ASSERT_EQ(0, sd_bus_message_seal(msg.get(), 0, 0));

    mctpDiscoveryHandler->removeEndpoints(msg);
    // Original entry should remain untouched
    EXPECT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 1);
}

TEST(MctpEndpointDiscoveryTest, removeEndpointsSameEidDifferentNetwork)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    pldm::MockManager manager;

    // Only the matching network+EID entry should be removed
    EXPECT_CALL(manager, handleRemovedMctpEndpoints(_)).Times(1);

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager});

    // EID 10 on network 1, EID 10 on network 2
    const pldm::MctpInfos& seedInfos = {
        pldm::MctpInfo(10, pldm::emptyUUID, "", 1, std::nullopt),
        pldm::MctpInfo(10, pldm::emptyUUID, "", 2, std::nullopt)};
    mctpDiscoveryHandler->addToExistingMctpInfos(seedInfos);
    ASSERT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 2);

    // Signal removes EID 10 on network 1 only
    auto msg = bus.new_signal("/test", "org.freedesktop.DBus.ObjectManager",
                              "InterfacesRemoved");
    sdbusplus::message::object_path objPath{
        "/au/com/codeconstruct/mctp1/networks/1/endpoints/10"};
    std::vector<std::string> interfaces{MCTPEndpoint::interface};
    msg.append(objPath, interfaces);
    ASSERT_EQ(0, sd_bus_message_seal(msg.get(), 0, 0));

    mctpDiscoveryHandler->removeEndpoints(msg);
    ASSERT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 1);
    // Remaining entry should be network 2
    EXPECT_EQ(std::get<3>(mctpDiscoveryHandler->existingMctpInfos[0]), 2);
}

TEST(MctpEndpointDiscoveryTest, removeEndpointsWrongInterface)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    pldm::MockManager manager;

    // Handler should NOT be called for non-MCTP interface removal
    EXPECT_CALL(manager, handleRemovedMctpEndpoints(_)).Times(0);

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager});

    const pldm::MctpInfos& seedInfos = {
        pldm::MctpInfo(10, pldm::emptyUUID, "", 1, std::nullopt)};
    mctpDiscoveryHandler->addToExistingMctpInfos(seedInfos);

    // Signal with unrelated interface - should be ignored
    auto msg = bus.new_signal("/test", "org.freedesktop.DBus.ObjectManager",
                              "InterfacesRemoved");
    sdbusplus::message::object_path objPath{
        "/au/com/codeconstruct/mctp1/networks/1/endpoints/10"};
    std::vector<std::string> interfaces{"xyz.openbmc_project.Inventory.Item"};
    msg.append(objPath, interfaces);
    ASSERT_EQ(0, sd_bus_message_seal(msg.get(), 0, 0));

    mctpDiscoveryHandler->removeEndpoints(msg);
    // Endpoint should remain since the signal was for a different interface
    EXPECT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 1);
}

TEST(MctpEndpointDiscoveryTest, removeEndpointsMalformedPath)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    pldm::MockManager manager;

    // Handler should NOT be called for malformed paths
    EXPECT_CALL(manager, handleRemovedMctpEndpoints(_)).Times(0);

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager});

    const pldm::MctpInfos& seedInfos = {
        pldm::MctpInfo(10, pldm::emptyUUID, "", 1, std::nullopt)};
    mctpDiscoveryHandler->addToExistingMctpInfos(seedInfos);

    // Signal with malformed object path (missing /endpoints/ segment)
    auto msg = bus.new_signal("/test", "org.freedesktop.DBus.ObjectManager",
                              "InterfacesRemoved");
    sdbusplus::message::object_path objPath{
        "/au/com/codeconstruct/mctp1/networks/1"};
    std::vector<std::string> interfaces{MCTPEndpoint::interface};
    msg.append(objPath, interfaces);
    ASSERT_EQ(0, sd_bus_message_seal(msg.get(), 0, 0));

    mctpDiscoveryHandler->removeEndpoints(msg);
    // Endpoint should remain since path couldn't be parsed
    EXPECT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 1);
}
