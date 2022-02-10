#include "libpldm/base.h"
#include "platform-mc/manager.hpp"
#include "platform-mc/test/mock_manager.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/mctp_endpoint_discovery.hpp"

#include <sdbusplus/timer.hpp>
#include <sdeventplus/event.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm::requester;
using namespace std::chrono;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Between;
using ::testing::Exactly;
using ::testing::NiceMock;
using ::testing::Return;

TEST(MctpEndpointDiscoveryTest, handleMCTPEndpoints)
{
    int fd = 1;
    sdeventplus::Event event(sdeventplus::Event::get_default());
    pldm::dbus_api::Requester dbusImplRequester(
        pldm::utils::DBusHandler::getBus(), "/syz/openbmc_project/pldm");
    pldm::requester::Handler<pldm::requester::Request> reqHandler(
        fd, event, dbusImplRequester, false, 90000, seconds(1), 2, milliseconds(100));

    auto& bus = pldm::utils::DBusHandler::getBus();
    pldm::platform_mc::MockManager manager(event, reqHandler,
                                           dbusImplRequester);
    auto mctpDiscoveryHandler = std::make_unique<pldm::MctpDiscovery>(
        bus, std::initializer_list<pldm::IMctpDiscoveryHandler*>{&manager});

    EXPECT_CALL(manager, handleMCTPEndpoints(_)).Times(1);
    mctpDiscoveryHandler->loadStaticEndpoints("../test/static_eid_table.json");
}