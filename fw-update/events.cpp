#include "events.hpp"

#include <phosphor-logging/commit.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Software/Update/event.hpp>

#include <optional>
#include <string_view>

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update::events
{

namespace error_intf = sdbusplus::error::xyz::openbmc_project::software::Update;
namespace event_intf = sdbusplus::event::xyz::openbmc_project::software::Update;

namespace
{

void logCommitFailure(std::string_view eventName,
                      const sdbusplus::object_path& targetPath,
                      const std::exception& e)
{
    error("Failed to commit {NAME} for target {TARGET}: {ERROR}", "NAME",
          eventName, "TARGET", targetPath.str, "ERROR", e);
}

/* The metadata of an event is ordered by the arguments of the Redfish message
   it maps on to, so the two fields do not appear in the same order for every
   event, and a generated constructor checks each name against the position
   its own event declares. Hence the two helpers below. */

template <typename EventType>
void generateTargetFirst(
    std::string_view eventName,
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier)
{
    if (!targetPath)
    {
        return;
    }

    try
    {
        lg2::commit(EventType("TARGET_NAME", *targetPath, "IMAGE_IDENTIFIER",
                              imageIdentifier));
    }
    catch (const std::exception& e)
    {
        logCommitFailure(eventName, *targetPath, e);
    }
}

template <typename EventType>
void generateImageFirst(std::string_view eventName,
                        const std::optional<sdbusplus::object_path>& targetPath,
                        const std::string& imageIdentifier)
{
    if (!targetPath)
    {
        return;
    }

    try
    {
        lg2::commit(EventType("IMAGE_IDENTIFIER", imageIdentifier,
                              "TARGET_NAME", *targetPath));
    }
    catch (const std::exception& e)
    {
        logCommitFailure(eventName, *targetPath, e);
    }
}

} // namespace

void generateTargetDetermined(
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier)
{
    generateTargetFirst<event_intf::TargetDetermined>(
        "TargetDetermined", targetPath, imageIdentifier);
}

void generateTransferringToComponent(
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier)
{
    generateImageFirst<event_intf::TransferringToComponent>(
        "TransferringToComponent", targetPath, imageIdentifier);
}

void generateVerifyingAtComponent(
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier)
{
    generateImageFirst<event_intf::VerifyingAtComponent>(
        "VerifyingAtComponent", targetPath, imageIdentifier);
}

void generateInstallingOnComponent(
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier)
{
    generateImageFirst<event_intf::InstallingOnComponent>(
        "InstallingOnComponent", targetPath, imageIdentifier);
}

void generateUpdateSuccessful(
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier)
{
    generateTargetFirst<event_intf::UpdateSuccessful>(
        "UpdateSuccessful", targetPath, imageIdentifier);
}

void generateVerificationFailed(
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier)
{
    generateImageFirst<error_intf::VerificationFailed>(
        "VerificationFailed", targetPath, imageIdentifier);
}

void generateTransferFailed(
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier)
{
    generateImageFirst<error_intf::TransferFailed>("TransferFailed", targetPath,
                                                   imageIdentifier);
}

void generateActivateFailed(
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier)
{
    generateImageFirst<error_intf::ActivateFailed>("ActivateFailed", targetPath,
                                                   imageIdentifier);
}

void generateUpdateNotApplicable(
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier)
{
    generateImageFirst<error_intf::UpdateNotApplicable>(
        "UpdateNotApplicable", targetPath, imageIdentifier);
}

} // namespace pldm::fw_update::events
