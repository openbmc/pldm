#include "fw-update/update_manager.hpp"
#include "test/test_instance_id.hpp"

#include <libpldm/firmware_update.h>
#include <sys/mman.h>
#include <unistd.h>

#include <xyz/openbmc_project/Common/error.hpp>

#include <array>
#include <fstream>
#include <iterator>
#include <vector>

#include <gtest/gtest.h>

using namespace pldm;
using namespace pldm::fw_update;
using namespace std::chrono;
namespace software = sdbusplus::xyz::openbmc_project::Software::server;
using InvalidArgument =
    sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument;
using Unavailable = sdbusplus::xyz::openbmc_project::Common::Error::Unavailable;

class PackageIntakeTest : public testing::Test
{
  protected:
    PackageIntakeTest() :
        event(sdeventplus::Event::get_default()),
        reqHandler(nullptr, event, instanceIdDb, false, seconds(1), 2,
                   milliseconds(100)),
        updateManager(event, reqHandler, instanceIdDb, descriptorMap,
                      componentInfoMap)
    {
        std::ifstream pkg("./test_pkg", std::ios::binary);
        package.assign(std::istreambuf_iterator<char>(pkg), {});
    }

    // memfd holding `bytes`, rewound to offset 0 like bmcweb hands it over
    static int makeMemfd(const std::vector<uint8_t>& bytes)
    {
        int fd = memfd_create("package_intake_test", 0);
        EXPECT_GE(fd, 0);
        EXPECT_EQ(write(fd, bytes.data(), bytes.size()),
                  static_cast<ssize_t>(bytes.size()));
        EXPECT_EQ(lseek(fd, 0, SEEK_SET), 0);
        return fd;
    }

    // Runs the deferred package processing scheduled by processFd()
    void runDeferred()
    {
        event.run(milliseconds(50));
    }

    // Response handler the requester keeps for a request of the updater
    DeviceUpdater::ResponseHandler outstandingResponse(mctp_eid_t eid)
    {
        return updateManager.deviceUpdaterMap.at(eid)->guarded(
            &DeviceUpdater::requestUpdate);
    }

    bool hasUpdater(mctp_eid_t eid) const
    {
        return updateManager.deviceUpdaterMap.contains(eid);
    }

    bool completionRecorded() const
    {
        return !updateManager.deviceUpdateCompletionMap.empty();
    }

    bool packageHeld() const
    {
        return updateManager.packageFd || updateManager.packageMap ||
               updateManager.packageStream;
    }

    bool updatersReleased() const
    {
        return updateManager.deviceUpdaterMap.empty() && !updateManager.parser;
    }

    sdeventplus::Event event;
    TestInstanceIdDb instanceIdDb;
    requester::Handler<requester::Request> reqHandler;
    DescriptorMap descriptorMap{
        {1,
         {{PLDM_FWUP_UUID,
           std::vector<uint8_t>{0x16, 0x20, 0x23, 0xC9, 0x3E, 0xC5, 0x41, 0x15,
                                0x95, 0xF4, 0x48, 0x70, 0x1D, 0x49, 0xD6,
                                0x75}}}}};
    ComponentInfoMap componentInfoMap{{1, {{std::make_pair(10, 100), 1}}}};
    UpdateManager updateManager;
    std::vector<uint8_t> package;
};

TEST_F(PackageIntakeTest, ServesImageFromMappingAfterCallerClosesFd)
{
    int fd = makeMemfd(package);
    auto objPath = updateManager.processFd(fd);
    // The D-Bus message owns the caller's descriptor and closes it on return
    close(fd);
    EXPECT_TRUE(objPath.starts_with(updateManager.swRootPath));
    EXPECT_TRUE(packageHeld());

    runDeferred();
    ASSERT_NE(updateManager.activation, nullptr);
#ifdef FW_UPDATE_INOTIFY_ENABLED
    EXPECT_EQ(updateManager.activation->activation(),
              software::Activation::Activations::Ready);
#else
    EXPECT_EQ(updateManager.activation->activation(),
              software::Activation::Activations::Activating);
#endif

    // RequestFirmwareData for the first 512 bytes of the only component
    constexpr std::array<uint8_t, sizeof(pldm_msg_hdr) +
                                      sizeof(pldm_request_firmware_data_req)>
        reqFwDataReq{0x8A, 0x05, 0x15, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x02, 0x00, 0x00};
    constexpr uint32_t length = 512;
    constexpr size_t compOffset = 139;
    auto response = updateManager.handleRequest(
        1, PLDM_REQUEST_FIRMWARE_DATA,
        reinterpret_cast<const pldm_msg*>(reqFwDataReq.data()),
        sizeof(pldm_request_firmware_data_req));
    ASSERT_EQ(response.size(), sizeof(pldm_msg_hdr) + sizeof(uint8_t) + length);
    EXPECT_EQ(response[sizeof(pldm_msg_hdr)], PLDM_SUCCESS);
    std::vector<uint8_t> expected(package.begin() + compOffset,
                                  package.begin() + compOffset + length);
    std::vector<uint8_t> served(
        response.begin() + sizeof(pldm_msg_hdr) + sizeof(uint8_t),
        response.end());
    EXPECT_EQ(served, expected);
}

TEST_F(PackageIntakeTest, DropsResponsesForAReleasedUpdater)
{
    int fd = makeMemfd(package);
    updateManager.processFd(fd);
    close(fd);
    runDeferred();
    ASSERT_TRUE(hasUpdater(1));
    auto response = outstandingResponse(1);

    // The next update releases the updater while its request is outstanding
    updateManager.resetActivationState();
    ASSERT_FALSE(hasUpdater(1));

    // The late response must not reach the released updater
    response(1, nullptr, 0);
    EXPECT_FALSE(completionRecorded());
}

TEST_F(PackageIntakeTest, RejectsEmptyImage)
{
    int fd = makeMemfd({});
    EXPECT_THROW(updateManager.processFd(fd), InvalidArgument);
    EXPECT_FALSE(packageHeld());
    close(fd);
}

TEST_F(PackageIntakeTest, RejectsNonRegularFile)
{
    std::array<int, 2> pipeFds{-1, -1};
    ASSERT_EQ(pipe(pipeFds.data()), 0);
    EXPECT_THROW(updateManager.processFd(pipeFds[0]), InvalidArgument);
    EXPECT_FALSE(packageHeld());
    close(pipeFds[0]);
    close(pipeFds[1]);
}

TEST_F(PackageIntakeTest, ReleasesMappingWhenNoDeviceIsDiscovered)
{
    descriptorMap.clear();
    int fd = makeMemfd(package);
    EXPECT_THROW(updateManager.processFd(fd), Unavailable);
    EXPECT_FALSE(packageHeld());
    close(fd);
}

TEST_F(PackageIntakeTest, ReleasesPackageAfterCompletion)
{
    int fd = makeMemfd(package);
    updateManager.processFd(fd);
    close(fd);
    runDeferred();
    ASSERT_TRUE(packageHeld());

    updateManager.updateDeviceCompletion(1, true);
    ASSERT_NE(updateManager.activation, nullptr);
    EXPECT_EQ(updateManager.activation->activation(),
              software::Activation::Activations::Active);
    // Released on the next loop iteration: the completing DeviceUpdater may
    // still be executing when the update completes
    EXPECT_TRUE(packageHeld());

    runDeferred();
    EXPECT_FALSE(packageHeld());
    EXPECT_TRUE(updatersReleased());
    // The Software object stays so clients can read the final state
    ASSERT_NE(updateManager.activation, nullptr);
    EXPECT_EQ(updateManager.activation->activation(),
              software::Activation::Activations::Active);

    // A late FD command is refused instead of being served from a released
    // package
    constexpr std::array<uint8_t, sizeof(pldm_msg_hdr) +
                                      sizeof(pldm_request_firmware_data_req)>
        reqFwDataReq{0x8A, 0x05, 0x15, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x02, 0x00, 0x00};
    auto response = updateManager.handleRequest(
        1, PLDM_REQUEST_FIRMWARE_DATA,
        reinterpret_cast<const pldm_msg*>(reqFwDataReq.data()),
        sizeof(pldm_request_firmware_data_req));
    ASSERT_EQ(response.size(), sizeof(pldm_msg_hdr) + sizeof(uint8_t));
    EXPECT_EQ(response[sizeof(pldm_msg_hdr)], PLDM_FWUP_COMMAND_NOT_EXPECTED);
}

TEST_F(PackageIntakeTest, ReleasesPackageWhenPackageIsInvalid)
{
    std::vector<uint8_t> truncated(package.begin(), package.begin() + 10);
    int fd = makeMemfd(truncated);
    updateManager.processFd(fd);
    close(fd);
    // The deferred processStream rejects the package
    runDeferred();

    EXPECT_FALSE(packageHeld());
    ASSERT_NE(updateManager.activation, nullptr);
    EXPECT_EQ(updateManager.activation->activation(),
              software::Activation::Activations::Invalid);
}

TEST_F(PackageIntakeTest, ReleasesPackageWhenNoDeviceMatches)
{
    descriptorMap[1] = {{PLDM_FWUP_UUID, std::vector<uint8_t>(16, 0xAA)}};
    int fd = makeMemfd(package);
    updateManager.processFd(fd);
    close(fd);
    runDeferred();

    EXPECT_FALSE(packageHeld());
    ASSERT_NE(updateManager.activation, nullptr);
    EXPECT_EQ(updateManager.activation->activation(),
              software::Activation::Activations::Invalid);
}

TEST_F(PackageIntakeTest, StartsTheNextUpdateBeforeAPendingRelease)
{
    int fd = makeMemfd(package);
    updateManager.processFd(fd);
    close(fd);
    runDeferred();
    updateManager.updateDeviceCompletion(1, true);
    ASSERT_TRUE(packageHeld());

    // The next StartUpdate is dispatched before the pending release runs
    updateManager.resetActivationState();
    int next = makeMemfd(package);
    updateManager.processFd(next);
    close(next);
    // One loop iteration dispatches one source: run both pending defers
    runDeferred();
    runDeferred();

    EXPECT_TRUE(packageHeld());
    EXPECT_TRUE(hasUpdater(1));
}
