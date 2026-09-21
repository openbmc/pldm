// Verifies that every generator commits its event with the expected name and
// metadata, against a stand-in for the phosphor-logging service.
//
// The stand-in serves its bus from a thread of its own, so the test thread
// blocks in the commit while the service thread answers it. A context is
// stopped from inside its own event loop, so one service runs for the whole
// binary and is left running at exit rather than being stopped per test.
#include "fw-update/events.hpp"

#include <sdbusplus/async.hpp>
#include <xyz/openbmc_project/Logging/Create/aserver.hpp>
#include <xyz/openbmc_project/Logging/Entry/aserver.hpp>
#include <xyz/openbmc_project/Software/Update/event.hpp>

#include <format>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace events = pldm::fw_update::events;

class TestEventServer;
class TestEventEntry;

using EventServerIntf =
    sdbusplus::aserver::xyz::openbmc_project::logging::Create<TestEventServer>;
using EventEntryIntf =
    sdbusplus::aserver::xyz::openbmc_project::logging::Entry<TestEventEntry>;

namespace error_intf = sdbusplus::error::xyz::openbmc_project::software::Update;
namespace event_intf = sdbusplus::event::xyz::openbmc_project::software::Update;

using AdditionalData = std::map<std::string, std::string>;

/** @brief A log entry created by the stand-in logging service */
class TestEventEntry : public EventEntryIntf
{
  public:
    TestEventEntry(sdbusplus::async::context& ctx, const char* path) :
        EventEntryIntf(ctx, path)
    {}

    static auto method_call(get_entry_t)
        -> sdbusplus::async::task<get_entry_t::return_type>
    {
        get_entry_t::return_type fd = 0;
        co_return fd;
    }
};

/** @brief Stands in for phosphor-logging, recording what was committed */
class TestEventServer : public EventServerIntf
{
  public:
    TestEventServer(sdbusplus::async::context& ctx, const char* path) :
        EventServerIntf(ctx, path), ctx(ctx)
    {}

    auto method_call(create_t, auto message, auto /*severity*/,
                     auto additionalData)
        -> sdbusplus::async::task<create_t::return_type>
    {
        static int count = 0;
        auto path =
            std::format("/xyz/openbmc_project/logging/entry/Test{}", ++count);
        {
            std::lock_guard<std::mutex> lock(mutex);
            commits.emplace_back(
                std::string(message),
                AdditionalData(additionalData.begin(), additionalData.end()));
        }
        entries.emplace_back(
            std::make_unique<TestEventEntry>(ctx, path.c_str()));

        co_return sdbusplus::object_path(path);
    }

    auto method_call(create_with_ffdc_files_t, auto, auto, auto, auto)
        -> sdbusplus::async::task<create_with_ffdc_files_t::return_type>
    {
        co_return sdbusplus::object_path{};
    }

    /** @brief Forget the events committed by the previous test */
    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex);
        commits.clear();
    }

    /** @brief The name and metadata of every event committed so far */
    auto committed() -> std::vector<std::pair<std::string, AdditionalData>>
    {
        std::lock_guard<std::mutex> lock(mutex);
        return commits;
    }

  private:
    sdbusplus::async::context& ctx;
    std::mutex mutex;
    std::vector<std::pair<std::string, AdditionalData>> commits;
    std::vector<std::unique_ptr<TestEventEntry>> entries;
};

/** @brief The stand-in logging service, running for the whole test binary */
class LoggingService
{
  public:
    static constexpr auto serviceName = "xyz.openbmc_project.Logging";
    static constexpr auto loggingPath = "/xyz/openbmc_project/logging";

    static auto instance() -> TestEventServer&
    {
        /* Deliberately leaked: ~context() throws when the context was not
           shut down, and a context only observes request_stop() from inside
           its own loop, which a synchronous commit never enters. */
        static auto* self = new LoggingService();
        return self->server;
    }

  private:
    LoggingService() : server(ctx, loggingPath), manager(ctx, loggingPath)
    {
        ctx.request_name(serviceName);
        std::thread([this] { ctx.run(); }).detach();
    }

    sdbusplus::async::context ctx;
    TestEventServer server;
    sdbusplus::server::manager_t manager;
};

class EventsTest : public testing::Test
{
  protected:
    static constexpr auto targetPath =
        "/xyz/openbmc_project/software/Harma_MB_CPLD_1737054000";
    static constexpr auto imageIdentifier = "FW_v1.0";

    void SetUp() override
    {
        eventServer.clear();
    }

    /** @brief Assert that exactly one event was committed, naming the target
     *
     *  @param[in] eventName - errName of the event expected
     *  @param[in] image - ImageIdentifier expected in the metadata
     */
    void expectOneCommit(std::string_view eventName, const std::string& image)
    {
        auto commits = eventServer.committed();
        ASSERT_EQ(commits.size(), 1);
        EXPECT_EQ(commits[0].first, eventName);
        EXPECT_EQ(commits[0].second.at("TARGET_NAME"), targetPath);
        EXPECT_EQ(commits[0].second.at("IMAGE_IDENTIFIER"), image);
    }

    TestEventServer& eventServer = LoggingService::instance();
};

TEST_F(EventsTest, CommitsTargetDetermined)
{
    events::generateTargetDetermined(sdbusplus::object_path(targetPath),
                                     imageIdentifier);
    expectOneCommit(event_intf::TargetDetermined::errName, imageIdentifier);
}

TEST_F(EventsTest, CommitsTransferringToComponent)
{
    events::generateTransferringToComponent(sdbusplus::object_path(targetPath),
                                            imageIdentifier);
    expectOneCommit(event_intf::TransferringToComponent::errName,
                    imageIdentifier);
}

TEST_F(EventsTest, CommitsVerifyingAtComponent)
{
    events::generateVerifyingAtComponent(sdbusplus::object_path(targetPath),
                                         imageIdentifier);
    expectOneCommit(event_intf::VerifyingAtComponent::errName, imageIdentifier);
}

TEST_F(EventsTest, CommitsInstallingOnComponent)
{
    events::generateInstallingOnComponent(sdbusplus::object_path(targetPath),
                                          imageIdentifier);
    expectOneCommit(event_intf::InstallingOnComponent::errName,
                    imageIdentifier);
}

TEST_F(EventsTest, CommitsUpdateSuccessful)
{
    events::generateUpdateSuccessful(sdbusplus::object_path(targetPath),
                                     imageIdentifier);
    expectOneCommit(event_intf::UpdateSuccessful::errName, imageIdentifier);
}

TEST_F(EventsTest, CommitsVerificationFailed)
{
    events::generateVerificationFailed(sdbusplus::object_path(targetPath),
                                       imageIdentifier);
    expectOneCommit(error_intf::VerificationFailed::errName, imageIdentifier);
}

TEST_F(EventsTest, CommitsTransferFailed)
{
    events::generateTransferFailed(sdbusplus::object_path(targetPath),
                                   imageIdentifier);
    expectOneCommit(error_intf::TransferFailed::errName, imageIdentifier);
}

TEST_F(EventsTest, CommitsActivateFailed)
{
    events::generateActivateFailed(sdbusplus::object_path(targetPath),
                                   imageIdentifier);
    expectOneCommit(error_intf::ActivateFailed::errName, imageIdentifier);
}

TEST_F(EventsTest, CommitsUpdateNotApplicable)
{
    events::generateUpdateNotApplicable(sdbusplus::object_path(targetPath),
                                        imageIdentifier);
    expectOneCommit(error_intf::UpdateNotApplicable::errName, imageIdentifier);
}

// A failure detected before the package header is parsed has no version to
// report, so the image identifier is committed empty rather than omitted.
TEST_F(EventsTest, CommitsFailureWithoutImageIdentifier)
{
    events::generateVerificationFailed(sdbusplus::object_path(targetPath), {});
    expectOneCommit(error_intf::VerificationFailed::errName, "");
}

// Every component of a package can fail the same way, so a repeated failure
// is committed each time rather than being suppressed.
TEST_F(EventsTest, CommitsRepeatedFailureEachTime)
{
    events::generateActivateFailed(sdbusplus::object_path(targetPath),
                                   imageIdentifier);
    events::generateActivateFailed(sdbusplus::object_path(targetPath),
                                   imageIdentifier);

    auto commits = eventServer.committed();
    ASSERT_EQ(commits.size(), 2);
    EXPECT_EQ(commits[0].first, error_intf::ActivateFailed::errName);
    EXPECT_EQ(commits[1].first, error_intf::ActivateFailed::errName);
}

// An update that names no software object, as the package path does, reports
// nothing rather than committing an event with an empty target.
TEST_F(EventsTest, CommitsNothingWithoutATarget)
{
    events::generateTargetDetermined(std::nullopt, imageIdentifier);
    events::generateTransferFailed(std::nullopt, imageIdentifier);
    events::generateUpdateSuccessful(std::nullopt, imageIdentifier);

    EXPECT_TRUE(eventServer.committed().empty());
}
