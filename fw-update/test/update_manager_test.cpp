#include "fw-update/update_manager.hpp"
#include "test/test_instance_id.hpp"

#include <libpldm/firmware_update.h>

#include <array>
#include <chrono>
#include <sstream>

#include <gtest/gtest.h>

using namespace pldm;
using namespace pldm::fw_update;
using namespace std::chrono;

class UpdateManagerErrorPathTest : public testing::Test
{
  protected:
    UpdateManagerErrorPathTest() :
        event(sdeventplus::Event::get_default()), instanceIdDb(),
        reqHandler(nullptr, event, instanceIdDb, false, seconds(1), 2,
                   milliseconds(100)),
        updateManager(event, reqHandler, instanceIdDb, descriptorMap,
                      componentInfoMap)
    {}

    sdeventplus::Event event;
    TestInstanceIdDb instanceIdDb;
    requester::Handler<requester::Request> reqHandler;
    DescriptorMap descriptorMap{};
    ComponentInfoMap componentInfoMap{};
    UpdateManager updateManager;
};

TEST_F(UpdateManagerErrorPathTest, ProcessStreamZeroLengthThrows)
{
    std::stringstream packageStream;

    EXPECT_ANY_THROW(updateManager.processStream(packageStream, 0));
}

TEST_F(UpdateManagerErrorPathTest, ProcessStreamShorterThanHeaderThrows)
{
    std::stringstream packageStream;
    constexpr uintmax_t packageSize =
        sizeof(pldm_package_header_information) - 1;

    EXPECT_ANY_THROW(updateManager.processStream(packageStream, packageSize));
}

TEST_F(UpdateManagerErrorPathTest, ProcessStreamDeferWithoutDevicesThrows)
{
    std::stringstream packageStream;
    constexpr uintmax_t deferredPackageSize = 16;

    EXPECT_THROW(
        updateManager.processStreamDefer(packageStream, deferredPackageSize),
        sdbusplus::xyz::openbmc_project::Common::Error::Unavailable);
}

TEST_F(UpdateManagerErrorPathTest,
       HandleRequestUnknownEidReturnsCommandNotExpected)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr)> requestBytes{};
    auto* request = new (requestBytes.data()) pldm_msg;

    request->hdr.instance_id = 9;
    request->hdr.type = PLDM_FWUP;
    request->hdr.command = PLDM_TRANSFER_COMPLETE;

    const auto response = updateManager.handleRequest(
        0x2A, PLDM_TRANSFER_COMPLETE, request, sizeof(pldm_msg_hdr));
    const auto* responseMsg =
        reinterpret_cast<const pldm_msg*>(response.data());

    EXPECT_EQ(response.size(), sizeof(pldm_msg));
    EXPECT_EQ(responseMsg->hdr.instance_id, request->hdr.instance_id);
    EXPECT_EQ(responseMsg->hdr.type, request->hdr.type);
    EXPECT_EQ(responseMsg->hdr.command, request->hdr.command);
    EXPECT_EQ(responseMsg->payload[0], PLDM_FWUP_COMMAND_NOT_EXPECTED);
}

TEST_F(UpdateManagerErrorPathTest,
       HandleRequestUnknownEidPreservesHeaderForUnsupportedCommand)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr)> requestBytes{};
    auto* request = new (requestBytes.data()) pldm_msg;

    request->hdr.instance_id = 3;
    request->hdr.type = PLDM_FWUP;
    request->hdr.command = PLDM_REQUEST_FIRMWARE_DATA;

    const auto response =
        updateManager.handleRequest(0x63, 0xFF, request, sizeof(pldm_msg_hdr));
    const auto* responseMsg =
        reinterpret_cast<const pldm_msg*>(response.data());

    EXPECT_EQ(response.size(), sizeof(pldm_msg));
    EXPECT_EQ(responseMsg->hdr.instance_id, request->hdr.instance_id);
    EXPECT_EQ(responseMsg->hdr.type, request->hdr.type);
    EXPECT_EQ(responseMsg->hdr.command, request->hdr.command);
    EXPECT_EQ(responseMsg->payload[0], PLDM_FWUP_COMMAND_NOT_EXPECTED);
}
