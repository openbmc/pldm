#include "common/test/mocked_utils.hpp"
#include "common/types.hpp"
#include "common/utils.hpp"
#include "requester/test/mock_mctp_discovery_handler_intf.hpp"
#include "requester/test/test_peer_id_policy.hpp"

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
    std::map<pldm::MctpInfo,
             std::pair<pldm::dbus::ObjectPath, pldm::Availability>>
        currentMctpInfoMap;

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager});
    mctpDiscoveryHandler->getMctpInfos(currentMctpInfoMap);
    EXPECT_EQ(currentMctpInfoMap.size(), 0);
}

TEST(MctpEndpointDiscoveryTest, goodAddToExistingMctpInfos)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    pldm::MockManager manager;
    const pldm::MctpInfosWithPath& mctpInfos = {
        {"/au/com/codeconstruct/mctp1/networks/1/endpoints/11",
         pldm::MctpInfo(11, pldm::emptyUUID, "", 1, std::nullopt)},
        {"/au/com/codeconstruct/mctp1/networks/1/endpoints/12",
         pldm::MctpInfo(12, pldm::emptyUUID, "abc", 1, std::nullopt)}};

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager});
    mctpDiscoveryHandler->addToExistingMctpInfos(mctpInfos);
    EXPECT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 2);
    pldm::MctpInfo mctpInfo =
        mctpDiscoveryHandler->existingMctpInfos.back().second;
    EXPECT_EQ(std::get<0>(mctpInfo), 12);
    EXPECT_EQ(std::get<2>(mctpInfo), "abc");
    EXPECT_EQ(std::get<3>(mctpInfo), 1);
}

TEST(MctpEndpointDiscoveryTest, badAddToExistingMctpInfos)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    pldm::MockManager manager;
    const pldm::MctpInfosWithPath& mctpInfos = {
        {"/au/com/codeconstruct/mctp1/networks/1/endpoints/11",
         pldm::MctpInfo(11, pldm::emptyUUID, "", 1, std::nullopt)}};

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager});
    mctpDiscoveryHandler->addToExistingMctpInfos(mctpInfos);
    EXPECT_NE(mctpDiscoveryHandler->existingMctpInfos.size(), 2);
}

TEST(MctpEndpointDiscoveryTest, addToExistingMctpInfosMatchesByPath)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    pldm::MockManager manager;
    const std::string pathA =
        "/au/com/codeconstruct/mctp1/networks/1/endpoints/11";
    const std::string pathB =
        "/au/com/codeconstruct/mctp1/networks/1/endpoints/12";

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager});
    mctpDiscoveryHandler->addToExistingMctpInfos(
        {{pathA, pldm::MctpInfo(11, pldm::emptyUUID, "", 1, std::nullopt)}});
    ASSERT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 1);

    // Same path with a different MctpInfo is the same endpoint, keep the
    // entry that is already tracked
    mctpDiscoveryHandler->addToExistingMctpInfos(
        {{pathA, pldm::MctpInfo(11, pldm::emptyUUID, "abc", 1, std::nullopt)}});
    ASSERT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 1);
    EXPECT_EQ(std::get<2>(mctpDiscoveryHandler->existingMctpInfos[0].second),
              "");

    // A different path with an identical MctpInfo is another endpoint
    mctpDiscoveryHandler->addToExistingMctpInfos(
        {{pathB, pldm::MctpInfo(11, pldm::emptyUUID, "", 1, std::nullopt)}});
    EXPECT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 2);
}

TEST(MctpEndpointDiscoveryTest, goodSearchConfigurationFor)
{
    MockdBusHandler mockedDbusHandler;
    auto& bus = mockedDbusHandler.getBus();
    pldm::MockManager manager;
    const pldm::MctpInfosWithPath& mctpInfos = {
        {"/au/com/codeconstruct/mctp1/networks/1/endpoints/10",
         pldm::MctpInfo(10, pldm::emptyUUID, "abc", 1, std::nullopt)}};

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
    pldm::MctpInfo mctpInfo =
        mctpDiscoveryHandler->existingMctpInfos.back().second;
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
    const pldm::MctpInfosWithPath& mctpInfos = {
        {"/au/com/codeconstruct/mctp1/networks/1/endpoints/10",
         pldm::MctpInfo(10, pldm::emptyUUID, "abc", 1, std::nullopt)}};

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
    pldm::MctpInfo mctpInfo =
        mctpDiscoveryHandler->existingMctpInfos.back().second;
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
    const pldm::MctpInfo endpointInfo(10, pldm::emptyUUID, "", 1, std::nullopt);

    // The handler should be told about exactly the removed endpoint
    EXPECT_CALL(manager,
                handleRemovedMctpEndpoints(pldm::MctpInfos{endpointInfo}))
        .Times(1);

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager});

    // Pre-populate existingMctpInfos
    const pldm::MctpInfosWithPath& seedInfos = {
        {"/au/com/codeconstruct/mctp1/networks/1/endpoints/10", endpointInfo}};
    mctpDiscoveryHandler->addToExistingMctpInfos(seedInfos);
    ASSERT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 1);
    // Announce the endpoint so that it is given a PeerId, as discovery does
    mctpDiscoveryHandler->handleMctpEndpoints(seedInfos);

    // Build InterfacesRemoved signal matching that endpoint
    auto msg = bus.new_signal("/test", "org.freedesktop.DBus.ObjectManager",
                              "InterfacesRemoved");
    sdbusplus::object_path objPath{
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

    // Handler should NOT be called when the path is not in existingMctpInfos
    EXPECT_CALL(manager, handleRemovedMctpEndpoints(_)).Times(0);

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager});

    // Seed with EID 10 on network 1
    const pldm::MctpInfosWithPath& seedInfos = {
        {"/au/com/codeconstruct/mctp1/networks/1/endpoints/10",
         pldm::MctpInfo(10, pldm::emptyUUID, "", 1, std::nullopt)}};
    mctpDiscoveryHandler->addToExistingMctpInfos(seedInfos);
    ASSERT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 1);

    // Signal removes EID 99 path which is not in existingMctpInfos
    auto msg = bus.new_signal("/test", "org.freedesktop.DBus.ObjectManager",
                              "InterfacesRemoved");
    sdbusplus::object_path objPath{
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
    const pldm::MctpInfo networkOneInfo(10, pldm::emptyUUID, "", 1,
                                        std::nullopt);
    const pldm::MctpInfo networkTwoInfo(10, pldm::emptyUUID, "", 2,
                                        std::nullopt);
    pldm::EndpointRegistry registry(
        std::make_unique<pldm::LowestFreePeerIdPolicy>());

    // Only the endpoint on the matching network should be removed
    EXPECT_CALL(manager,
                handleRemovedMctpEndpoints(pldm::MctpInfos{networkOneInfo}))
        .Times(1);

    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager},
        &registry);

    // EID 10 on network 1, EID 10 on network 2
    ASSERT_TRUE(registry.acquire(pldm::toEndpoint(networkOneInfo)));
    ASSERT_TRUE(registry.acquire(pldm::toEndpoint(networkTwoInfo)));
    const pldm::MctpInfosWithPath& seedInfos = {
        {"/au/com/codeconstruct/mctp1/networks/1/endpoints/10", networkOneInfo},
        {"/au/com/codeconstruct/mctp1/networks/2/endpoints/10",
         networkTwoInfo}};
    mctpDiscoveryHandler->addToExistingMctpInfos(seedInfos);
    ASSERT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 2);

    // Signal removes EID 10 on network 1 only
    auto msg = bus.new_signal("/test", "org.freedesktop.DBus.ObjectManager",
                              "InterfacesRemoved");
    sdbusplus::object_path objPath{
        "/au/com/codeconstruct/mctp1/networks/1/endpoints/10"};
    std::vector<std::string> interfaces{MCTPEndpoint::interface};
    msg.append(objPath, interfaces);
    ASSERT_EQ(0, sd_bus_message_seal(msg.get(), 0, 0));

    mctpDiscoveryHandler->removeEndpoints(msg);
    ASSERT_EQ(mctpDiscoveryHandler->existingMctpInfos.size(), 1);
    // Remaining entry should be network 2
    EXPECT_EQ(std::get<3>(mctpDiscoveryHandler->existingMctpInfos[0].second),
              2);
}

namespace
{
class PeerAwareHandler : public pldm::MctpDiscoveryHandlerIntf
{
  public:
    void handleMctpEndpoints(const pldm::MctpInfos&) override {}
    void handleRemovedMctpEndpoints(const pldm::MctpInfos&) override {}
    void updateMctpEndpointAvailability(
        const pldm::MctpInfo& mctpInfo,
        pldm::Availability availability) override
    {
        availabilities.emplace_back(pldm::toEndpoint(mctpInfo), availability);
    }
    std::optional<pldm::PeerId> getActivePeerIdByName(
        const std::string&) override
    {
        return std::nullopt;
    }

    void handleEndpoints(
        const std::vector<pldm::DiscoveredEndpoint>& endpoints) override
    {
        added.insert(added.end(), endpoints.begin(), endpoints.end());
    }

    void handleRemovedEndpoints(
        const std::vector<pldm::DiscoveredEndpoint>& endpoints) override
    {
        removed.insert(removed.end(), endpoints.begin(), endpoints.end());
    }

    std::vector<pldm::DiscoveredEndpoint> added;
    std::vector<pldm::DiscoveredEndpoint> removed;
    std::vector<std::pair<pldm::Endpoint, pldm::Availability>> availabilities;
};
} // namespace

TEST(MctpEndpointDiscoveryTest, handlersReceiveThePeerIdOfEachEndpoint)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    PeerAwareHandler handler;
    pldm::EndpointRegistry registry(
        std::make_unique<pldm::LowestFreePeerIdPolicy>());
    const pldm::MctpInfo networkOneInfo(10, pldm::emptyUUID, "", 1,
                                        std::nullopt);
    const pldm::MctpInfo networkTwoInfo(10, pldm::emptyUUID, "", 2,
                                        std::nullopt);

    pldm::MctpDiscovery discovery(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&handler},
        &registry);
    discovery.handleMctpEndpoints(
        {{"/au/com/codeconstruct/mctp1/networks/1/endpoints/10",
          networkOneInfo},
         {"/au/com/codeconstruct/mctp1/networks/2/endpoints/10",
          networkTwoInfo}});

    ASSERT_EQ(handler.added.size(), 2);
    EXPECT_NE(handler.added[0].peer, handler.added[1].peer);
    EXPECT_EQ(registry.endpointOf(handler.added[0].peer),
              pldm::toEndpoint(networkOneInfo));
    EXPECT_EQ(registry.endpointOf(handler.added[1].peer),
              pldm::toEndpoint(networkTwoInfo));
}

TEST(MctpEndpointDiscoveryTest, endpointWithoutPeerIdIsNotGivenToHandlers)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    pldm::MockManager manager;
    const pldm::MctpInfo networkOneInfo(10, pldm::emptyUUID, "", 1,
                                        std::nullopt);
    const pldm::MctpInfo networkTwoInfo(10, pldm::emptyUUID, "", 2,
                                        std::nullopt);
    const pldm::MctpInfo reservedInfo(3, pldm::emptyUUID, "", 1, std::nullopt);

    // The registry of MctpDiscovery maps the PeerId to the EID, so the second
    // Network cannot use EID 10 and EID 3 is reserved
    EXPECT_CALL(manager, handleMctpEndpoints(::testing::IsEmpty())).Times(1);
    EXPECT_CALL(manager, handleMctpEndpoints(pldm::MctpInfos{networkOneInfo}))
        .Times(1);

    pldm::MctpDiscovery discovery(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&manager});
    discovery.handleMctpEndpoints(
        {{"/au/com/codeconstruct/mctp1/networks/1/endpoints/10",
          networkOneInfo},
         {"/au/com/codeconstruct/mctp1/networks/2/endpoints/10",
          networkTwoInfo},
         {"/au/com/codeconstruct/mctp1/networks/1/endpoints/3", reservedInfo}});
}

TEST(MctpEndpointDiscoveryTest, removalGivesHandlersThePeerIdThenReleasesIt)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    PeerAwareHandler handler;
    pldm::EndpointRegistry registry(
        std::make_unique<pldm::IdentityPeerIdPolicy>());
    const std::string path =
        "/au/com/codeconstruct/mctp1/networks/1/endpoints/10";
    const pldm::MctpInfo info(10, pldm::emptyUUID, "", 1, std::nullopt);

    pldm::MctpDiscovery discovery(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&handler},
        &registry);
    discovery.addToExistingMctpInfos({{path, info}});
    discovery.handleMctpEndpoints({{path, info}});
    ASSERT_TRUE(registry.peerOf(pldm::toEndpoint(info)));

    auto msg = bus.new_signal("/test", "org.freedesktop.DBus.ObjectManager",
                              "InterfacesRemoved");
    std::vector<std::string> interfaces{MCTPEndpoint::interface};
    msg.append(sdbusplus::object_path{path}, interfaces);
    ASSERT_EQ(0, sd_bus_message_seal(msg.get(), 0, 0));
    discovery.removeEndpoints(msg);

    ASSERT_EQ(handler.removed.size(), 1);
    EXPECT_EQ(handler.removed[0].peer, 10);
    EXPECT_EQ(registry.peerOf(pldm::toEndpoint(info)), std::nullopt);
}

TEST(MctpEndpointDiscoveryTest, sameEidOnTwoNetworksSurvivesUnplugAndReplug)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    PeerAwareHandler handler;
    pldm::FakeMapper mapper;
    pldm::EndpointRegistry registry(
        std::make_unique<pldm::PooledPeerIdPolicy>(), &mapper);
    const std::string pathOne =
        "/au/com/codeconstruct/mctp1/networks/1/endpoints/10";
    const std::string pathTwo =
        "/au/com/codeconstruct/mctp1/networks/2/endpoints/10";
    const pldm::MctpInfo infoOne(10, "uuid1", "", 1, std::nullopt);
    const pldm::MctpInfo infoTwo(10, "uuid2", "", 2, std::nullopt);

    pldm::MctpDiscovery discovery(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&handler},
        &registry);
    auto remove = [&](const std::string& path) {
        auto msg = bus.new_signal("/test", "org.freedesktop.DBus.ObjectManager",
                                  "InterfacesRemoved");
        std::vector<std::string> interfaces{MCTPEndpoint::interface};
        msg.append(sdbusplus::object_path{path}, interfaces);
        ASSERT_EQ(0, sd_bus_message_seal(msg.get(), 0, 0));
        discovery.removeEndpoints(msg);
    };

    // Both endpoints are bound, the second one to a PeerId other than its EID
    discovery.addToExistingMctpInfos({{pathOne, infoOne}, {pathTwo, infoTwo}});
    discovery.handleMctpEndpoints({{pathOne, infoOne}, {pathTwo, infoTwo}});
    ASSERT_EQ(handler.added.size(), 2);
    const auto peerOne = handler.added[0].peer;
    const auto peerTwo = handler.added[1].peer;
    EXPECT_EQ(peerOne, 10);
    EXPECT_NE(peerTwo, peerOne);
    EXPECT_EQ(mapper.mapped.at(peerOne), pldm::toEndpoint(infoOne));
    EXPECT_EQ(mapper.mapped.at(peerTwo), pldm::toEndpoint(infoTwo));

    // Unplugging one leaves the other as it was
    remove(pathOne);
    ASSERT_EQ(handler.removed.size(), 1);
    EXPECT_EQ(handler.removed[0].peer, peerOne);
    EXPECT_FALSE(mapper.mapped.contains(peerOne));
    EXPECT_EQ(mapper.mapped.at(peerTwo), pldm::toEndpoint(infoTwo));
    EXPECT_EQ(registry.peerOf(pldm::toEndpoint(infoTwo)), peerTwo);

    // Plugging it in again binds it again, to the EID as before
    handler.added.clear();
    discovery.addToExistingMctpInfos({{pathOne, infoOne}});
    discovery.handleMctpEndpoints({{pathOne, infoOne}});
    ASSERT_EQ(handler.added.size(), 1);
    EXPECT_EQ(handler.added[0].peer, peerOne);
    EXPECT_EQ(mapper.mapped.at(peerOne), pldm::toEndpoint(infoOne));
    EXPECT_EQ(mapper.mapped.size(), 2);
}

TEST(MctpEndpointDiscoveryTest, endpointThatCannotBeBoundIsNotGivenToHandlers)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    PeerAwareHandler handler;
    pldm::FakeMapper mapper;
    mapper.failMap = true;
    pldm::EndpointRegistry registry(
        std::make_unique<pldm::PooledPeerIdPolicy>(), &mapper);
    const pldm::MctpInfo info(10, "uuid", "", 1, std::nullopt);

    pldm::MctpDiscovery discovery(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&handler},
        &registry);
    discovery.handleMctpEndpoints(
        {{"/au/com/codeconstruct/mctp1/networks/1/endpoints/10", info}});

    EXPECT_TRUE(handler.added.empty());
    EXPECT_EQ(registry.peerOf(pldm::toEndpoint(info)), std::nullopt);
}

namespace
{
constexpr auto pathNetworkOne =
    "/au/com/codeconstruct/mctp1/networks/1/endpoints/10";
constexpr auto pathNetworkTwo =
    "/au/com/codeconstruct/mctp1/networks/2/endpoints/10";
constexpr auto pathNetworkThree =
    "/au/com/codeconstruct/mctp1/networks/3/endpoints/10";

void removeEndpoint(pldm::MctpDiscovery& discovery, const std::string& path)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    auto msg = bus.new_signal("/test", "org.freedesktop.DBus.ObjectManager",
                              "InterfacesRemoved");
    std::vector<std::string> interfaces{MCTPEndpoint::interface};
    msg.append(sdbusplus::object_path{path}, interfaces);
    ASSERT_EQ(0, sd_bus_message_seal(msg.get(), 0, 0));
    discovery.removeEndpoints(msg);
}

void changeConnectivity(pldm::MctpDiscovery& discovery, const std::string& path,
                        const std::string& value)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    auto msg = bus.new_signal(path.c_str(), "org.freedesktop.DBus.Properties",
                              "PropertiesChanged");
    std::map<std::string, std::variant<std::string>> changed{
        {"Connectivity", value}};
    msg.append(std::string(pldm::MCTPInterfaceCC), changed);
    ASSERT_EQ(0, sd_bus_message_seal(msg.get(), 0, 0));
    discovery.propertiesChangedCb(msg);
}

class UuidGroupingTest : public ::testing::Test
{
  protected:
    UuidGroupingTest() :
        registry(std::make_unique<pldm::PooledPeerIdPolicy>(), &mapper),
        discovery(
            pldm::utils::DBusHandler::getBus(),
            std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&handler},
            &registry, true)
    {}

    // The same device, with the same EID, on the networks 1, 2 and 3
    void discover(const std::string& path, const pldm::MctpInfo& info)
    {
        discovery.addToExistingMctpInfos({{path, info}});
        discovery.handleMctpEndpoints({{path, info}});
    }

    pldm::MctpInfo onNetwork(pldm::NetworkId network,
                             const std::string& uuid = "same-device")
    {
        return pldm::MctpInfo(10, uuid, "", network, std::nullopt);
    }

    PeerAwareHandler handler;
    pldm::FakeMapper mapper;
    pldm::EndpointRegistry registry;
    pldm::MctpDiscovery discovery;
};
} // namespace

TEST_F(UuidGroupingTest, theEndpointDiscoveredFirstIsTheOneGiven)
{
    discover(pathNetworkOne, onNetwork(1));
    discover(pathNetworkTwo, onNetwork(2));

    ASSERT_EQ(handler.added.size(), 1);
    EXPECT_EQ(pldm::toEndpoint(handler.added[0].mctpInfo),
              (pldm::Endpoint{1, 10}));
    // The one that stands by has no PeerId and is not bound in the transport
    EXPECT_EQ(registry.peerOf(pldm::Endpoint{2, 10}), std::nullopt);
    EXPECT_EQ(mapper.mapped.size(), 1);
}

TEST_F(UuidGroupingTest, theNextEndpointTakesOverWhenTheActiveOneIsRemoved)
{
    discover(pathNetworkOne, onNetwork(1));
    discover(pathNetworkTwo, onNetwork(2));
    discover(pathNetworkThree, onNetwork(3));
    ASSERT_EQ(handler.added.size(), 1);
    handler.added.clear();

    removeEndpoint(discovery, pathNetworkOne);

    ASSERT_EQ(handler.removed.size(), 1);
    EXPECT_EQ(pldm::toEndpoint(handler.removed[0].mctpInfo),
              (pldm::Endpoint{1, 10}));
    // The one discovered first of those that stood by, with its own EID
    // as the PeerId that was just freed
    ASSERT_EQ(handler.added.size(), 1);
    EXPECT_EQ(pldm::toEndpoint(handler.added[0].mctpInfo),
              (pldm::Endpoint{2, 10}));
    EXPECT_EQ(handler.added[0].peer, 10);
    EXPECT_EQ(registry.peerOf(pldm::Endpoint{3, 10}), std::nullopt);
    EXPECT_EQ(mapper.mapped.size(), 1);
    EXPECT_EQ(mapper.mapped.at(10), (pldm::Endpoint{2, 10}));
}

TEST_F(UuidGroupingTest, removingAnEndpointThatStandsByIsNotReported)
{
    discover(pathNetworkOne, onNetwork(1));
    discover(pathNetworkTwo, onNetwork(2));
    handler.added.clear();

    removeEndpoint(discovery, pathNetworkTwo);

    EXPECT_TRUE(handler.removed.empty());
    EXPECT_TRUE(handler.added.empty());

    // Nothing is left to take over when the active one goes
    removeEndpoint(discovery, pathNetworkOne);
    ASSERT_EQ(handler.removed.size(), 1);
    EXPECT_TRUE(handler.added.empty());
}

TEST_F(UuidGroupingTest, theLastEndpointCanComeBackAfterAllWereRemoved)
{
    discover(pathNetworkOne, onNetwork(1));
    removeEndpoint(discovery, pathNetworkOne);
    handler.added.clear();

    discover(pathNetworkTwo, onNetwork(2));

    ASSERT_EQ(handler.added.size(), 1);
    EXPECT_EQ(pldm::toEndpoint(handler.added[0].mctpInfo),
              (pldm::Endpoint{2, 10}));
}

TEST_F(UuidGroupingTest, endpointsWithoutAUuidAreNeverGrouped)
{
    discover(pathNetworkOne, onNetwork(1, pldm::emptyUUID));
    discover(pathNetworkTwo, onNetwork(2, pldm::emptyUUID));

    EXPECT_EQ(handler.added.size(), 2);
}

TEST_F(UuidGroupingTest, differentUuidsAreDifferentDevices)
{
    discover(pathNetworkOne, onNetwork(1, "device-a"));
    discover(pathNetworkTwo, onNetwork(2, "device-b"));

    EXPECT_EQ(handler.added.size(), 2);
}

TEST_F(UuidGroupingTest, aLossOfConnectivityDoesNotSwitchTheDevice)
{
    discover(pathNetworkOne, onNetwork(1));
    discover(pathNetworkTwo, onNetwork(2));
    handler.added.clear();

    changeConnectivity(discovery, pathNetworkOne, "Unavailable");

    // The device is not removed, e.g. a firmware update on it goes on. The
    // handlers hear that it is not available, as without grouping.
    EXPECT_TRUE(handler.removed.empty());
    EXPECT_TRUE(handler.added.empty());
    ASSERT_EQ(handler.availabilities.size(), 1);
    EXPECT_EQ(handler.availabilities[0],
              std::make_pair(pldm::Endpoint{1, 10}, false));
    EXPECT_EQ(registry.peerOf(pldm::Endpoint{1, 10}), 10);
    EXPECT_EQ(registry.peerOf(pldm::Endpoint{2, 10}), std::nullopt);

    // and when it is available again
    changeConnectivity(discovery, pathNetworkOne, "Available");
    ASSERT_EQ(handler.availabilities.size(), 2);
    EXPECT_EQ(handler.availabilities[1],
              std::make_pair(pldm::Endpoint{1, 10}, true));
}

TEST_F(UuidGroupingTest,
       aNewEndpointDoesNotTakeOverWhenTheOneInUseIsNotAvailable)
{
    discover(pathNetworkOne, onNetwork(1));
    changeConnectivity(discovery, pathNetworkOne, "Unavailable");
    handler.added.clear();
    handler.availabilities.clear();

    discover(pathNetworkTwo, onNetwork(2));

    EXPECT_TRUE(handler.removed.empty());
    EXPECT_TRUE(handler.added.empty());
    EXPECT_EQ(registry.peerOf(pldm::Endpoint{2, 10}), std::nullopt);
}

TEST_F(UuidGroupingTest, anEndpointThatStandsByDoesNotMatterWhileTheOneInUseIs)
{
    discover(pathNetworkOne, onNetwork(1));
    discover(pathNetworkTwo, onNetwork(2));
    handler.added.clear();

    changeConnectivity(discovery, pathNetworkTwo, "Unavailable");
    changeConnectivity(discovery, pathNetworkTwo, "Available");

    EXPECT_TRUE(handler.removed.empty());
    EXPECT_TRUE(handler.added.empty());
    // The handlers do not know the endpoint that stands by
    EXPECT_TRUE(handler.availabilities.empty());
}

TEST_F(UuidGroupingTest, anAvailableEndpointIsUsedBeforeOneThatIsNotOnRemoval)
{
    discover(pathNetworkOne, onNetwork(1));
    discover(pathNetworkTwo, onNetwork(2));
    discover(pathNetworkThree, onNetwork(3));
    changeConnectivity(discovery, pathNetworkTwo, "Unavailable");
    handler.added.clear();

    removeEndpoint(discovery, pathNetworkOne);

    // The one that was discovered first is not available, so the next one is
    ASSERT_EQ(handler.added.size(), 1);
    EXPECT_EQ(pldm::toEndpoint(handler.added[0].mctpInfo),
              (pldm::Endpoint{3, 10}));
}

TEST_F(UuidGroupingTest, anEndpointThatIsNotAvailableTakesOverWhenItIsAvailable)
{
    discover(pathNetworkOne, onNetwork(1));
    discover(pathNetworkTwo, onNetwork(2));
    changeConnectivity(discovery, pathNetworkTwo, "Unavailable");
    handler.added.clear();

    removeEndpoint(discovery, pathNetworkOne);

    // Nothing can be reached, so the handlers are not given the endpoint
    ASSERT_EQ(handler.removed.size(), 1);
    EXPECT_TRUE(handler.added.empty());
    EXPECT_EQ(registry.peerOf(pldm::Endpoint{2, 10}), std::nullopt);
    EXPECT_TRUE(mapper.mapped.empty());

    changeConnectivity(discovery, pathNetworkTwo, "Available");

    // It is discovered as soon as it can be reached
    ASSERT_EQ(handler.added.size(), 1);
    EXPECT_EQ(pldm::toEndpoint(handler.added[0].mctpInfo),
              (pldm::Endpoint{2, 10}));
    EXPECT_EQ(handler.added[0].peer, 10);
    EXPECT_EQ(mapper.mapped.at(10), (pldm::Endpoint{2, 10}));
}

namespace
{

// MctpDiscovery with a check that the test answers when it chooses to, like the
// endpoint would
class UuidCheckTest : public ::testing::Test
{
  protected:
    struct Check
    {
        pldm::PeerId peer;
        std::function<void(bool)> answered;
    };

    UuidCheckTest() :
        registry(std::make_unique<pldm::PooledPeerIdPolicy>(), &mapper),
        discovery(
            pldm::utils::DBusHandler::getBus(),
            std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&handler},
            &registry, true,
            [this](pldm::PeerId peer, std::function<void(bool)> answered) {
                checks.push_back({peer, std::move(answered)});
            })
    {}

    void discover(const std::string& path, const pldm::MctpInfo& info)
    {
        discovery.addToExistingMctpInfos({{path, info}});
        discovery.handleMctpEndpoints({{path, info}});
    }

    pldm::MctpInfo onNetwork(pldm::NetworkId network)
    {
        return pldm::MctpInfo(10, "same-device", "", network, std::nullopt);
    }

    void answer(size_t index, bool answered)
    {
        // A copy, as answering can start another check
        auto callback = checks.at(index).answered;
        callback(answered);
    }

    PeerAwareHandler handler;
    pldm::FakeMapper mapper;
    pldm::EndpointRegistry registry;
    std::vector<Check> checks;
    pldm::MctpDiscovery discovery;
};
} // namespace

TEST_F(UuidCheckTest, anEndpointIsGivenToTheHandlersOnceItAnswers)
{
    discover(pathNetworkOne, onNetwork(1));

    // It is asked first
    ASSERT_EQ(checks.size(), 1);
    EXPECT_EQ(checks[0].peer, 10);
    EXPECT_TRUE(handler.added.empty());

    answer(0, true);

    ASSERT_EQ(handler.added.size(), 1);
    EXPECT_EQ(pldm::toEndpoint(handler.added[0].mctpInfo),
              (pldm::Endpoint{1, 10}));
    EXPECT_EQ(handler.added[0].peer, 10);
}

TEST_F(UuidCheckTest, theNextEndpointIsTriedWhenOneDoesNotAnswer)
{
    discover(pathNetworkOne, onNetwork(1));
    discover(pathNetworkTwo, onNetwork(2));
    // The second one stands by while the first one is asked
    ASSERT_EQ(checks.size(), 1);

    answer(0, false);

    // The first one is put back on standby, the second one is asked
    EXPECT_TRUE(handler.added.empty());
    ASSERT_EQ(checks.size(), 2);
    EXPECT_EQ(registry.peerOf(pldm::Endpoint{1, 10}), std::nullopt);
    EXPECT_EQ(mapper.mapped.at(10), (pldm::Endpoint{2, 10}));

    answer(1, true);

    ASSERT_EQ(handler.added.size(), 1);
    EXPECT_EQ(pldm::toEndpoint(handler.added[0].mctpInfo),
              (pldm::Endpoint{2, 10}));
    EXPECT_EQ(handler.added[0].peer, 10);
}

TEST_F(UuidCheckTest, noEndpointIsGivenToTheHandlersWhenNoneAnswers)
{
    discover(pathNetworkOne, onNetwork(1));
    discover(pathNetworkTwo, onNetwork(2));

    answer(0, false);
    answer(1, false);

    // None is left to ask, and none is bound in the transport
    EXPECT_TRUE(handler.added.empty());
    EXPECT_EQ(checks.size(), 2);
    EXPECT_TRUE(mapper.mapped.empty());
    EXPECT_EQ(registry.peerOf(pldm::Endpoint{1, 10}), std::nullopt);
    EXPECT_EQ(registry.peerOf(pldm::Endpoint{2, 10}), std::nullopt);
}

TEST_F(UuidCheckTest, anEndpointThatDidNotAnswerIsAskedAgainWhenItIsAvailable)
{
    discover(pathNetworkOne, onNetwork(1));
    answer(0, false);
    ASSERT_EQ(checks.size(), 1);

    changeConnectivity(discovery, pathNetworkOne, "Unavailable");
    changeConnectivity(discovery, pathNetworkOne, "Available");

    ASSERT_EQ(checks.size(), 2);
    EXPECT_TRUE(handler.added.empty());

    answer(1, true);

    ASSERT_EQ(handler.added.size(), 1);
    EXPECT_EQ(pldm::toEndpoint(handler.added[0].mctpInfo),
              (pldm::Endpoint{1, 10}));
}

TEST_F(UuidCheckTest, anEndpointRemovedWhileItIsCheckedIsNotGivenToTheHandlers)
{
    discover(pathNetworkOne, onNetwork(1));

    removeEndpoint(discovery, pathNetworkOne);

    // The handlers never knew it, and its PeerId is free again
    EXPECT_TRUE(handler.removed.empty());
    EXPECT_EQ(registry.peerOf(pldm::Endpoint{1, 10}), std::nullopt);

    // A late answer is ignored
    answer(0, true);

    EXPECT_TRUE(handler.added.empty());
}

TEST(MctpEndpointDiscoveryTest, endpointsWithTheSameUuidAreNotGroupedByDefault)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    PeerAwareHandler handler;
    pldm::EndpointRegistry registry(
        std::make_unique<pldm::PooledPeerIdPolicy>());
    const pldm::MctpInfo infoOne(10, "same-device", "", 1, std::nullopt);
    const pldm::MctpInfo infoTwo(10, "same-device", "", 2, std::nullopt);

    pldm::MctpDiscovery discovery(
        bus, std::initializer_list<pldm::MctpDiscoveryHandlerIntf*>{&handler},
        &registry);
    discovery.handleMctpEndpoints(
        {{pathNetworkOne, infoOne}, {pathNetworkTwo, infoTwo}});

    EXPECT_EQ(handler.added.size(), 2);
}
