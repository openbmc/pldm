#include "libpldm/base.h"

#include "platform-mc/manager.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/mctp_endpoint_discovery.hpp"

#include <sdbusplus/timer.hpp>
#include <sdeventplus/event.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace std::chrono;

using ::testing::AtLeast;
using ::testing::Between;
using ::testing::Exactly;
using ::testing::NiceMock;
using ::testing::Return;

class TerminusManagerTest : public testing::Test
{
  protected:
    TerminusManagerTest() :
        bus(pldm::utils::DBusHandler::getBus()),
        event(sdeventplus::Event::get_default()),
        dbusImplRequester(bus, "/xyz/openbmc_project/pldm"),
        reqHandler(fd, event, dbusImplRequester, false, 90000, seconds(1), 2,
                   milliseconds(100)),
        terminusManager(event, reqHandler, dbusImplRequester, terminuses,
                        nullptr)
    {}

    int fd = -1;
    sdbusplus::bus::bus& bus;
    sdeventplus::Event event;
    pldm::dbus_api::Requester dbusImplRequester;
    pldm::requester::Handler<pldm::requester::Request> reqHandler;
    pldm::platform_mc::TerminusManager terminusManager;
    std::vector<std::shared_ptr<pldm::platform_mc::Terminus>> terminuses;
};

TEST_F(TerminusManagerTest, sendGetTID)
{
    constexpr size_t lengthRespGetTID = 2;
    constexpr std::array<uint8_t, sizeof(pldm_msg_hdr) + lengthRespGetTID>
        respGetTID{0x00, 0x00, 0x00, 0x00, 0x00};
    auto msgRespGetTID = reinterpret_cast<const pldm_msg*>(respGetTID.data());

    terminusManager.handleRespGetTID(1, msgRespGetTID, lengthRespGetTID);
    auto t = terminusManager.getMappedTID(1);

    EXPECT_EQ(t, std::nullopt);
}