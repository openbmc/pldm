#include "events.hpp"

#include <phosphor-logging/commit.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Software/Update/event.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

namespace error_intf = sdbusplus::error::xyz::openbmc_project::software::Update;
namespace event_intf = sdbusplus::event::xyz::openbmc_project::software::Update;

/* A generated constructor checks each metadata name against the position the
   event declares it at in Update.events.yaml, and the events do not all
   declare the two fields in the same order, hence the two helpers below. */

template <typename EventType>
void Events::generateTargetFirst(std::string_view eventName,
                                 const std::string& imageIdentifier)
{
    try
    {
        lg2::commit(EventType("TARGET_NAME", targetPath, "IMAGE_IDENTIFIER",
                              imageIdentifier));
    }
    catch (const std::exception& e)
    {
        logCommitFailure(eventName, e);
    }
}

template <typename EventType>
void Events::generateImageFirst(std::string_view eventName,
                                const std::string& imageIdentifier)
{
    try
    {
        lg2::commit(EventType("IMAGE_IDENTIFIER", imageIdentifier,
                              "TARGET_NAME", targetPath));
    }
    catch (const std::exception& e)
    {
        logCommitFailure(eventName, e);
    }
}

void Events::logCommitFailure(std::string_view eventName,
                              const std::exception& e) const
{
    error("Failed to commit {NAME} for target {TARGET}: {ERROR}", "NAME",
          eventName, "TARGET", targetPath.str, "ERROR", e);
}

void Events::generateTargetDetermined(const std::string& imageIdentifier)
{
    generateTargetFirst<event_intf::TargetDetermined>("TargetDetermined",
                                                      imageIdentifier);
}

void Events::generateTransferringToComponent(const std::string& imageIdentifier)
{
    generateImageFirst<event_intf::TransferringToComponent>(
        "TransferringToComponent", imageIdentifier);
}

void Events::generateVerifyingAtComponent(const std::string& imageIdentifier)
{
    generateImageFirst<event_intf::VerifyingAtComponent>("VerifyingAtComponent",
                                                         imageIdentifier);
}

void Events::generateInstallingOnComponent(const std::string& imageIdentifier)
{
    generateImageFirst<event_intf::InstallingOnComponent>(
        "InstallingOnComponent", imageIdentifier);
}

void Events::generateUpdateSuccessful(const std::string& imageIdentifier)
{
    generateTargetFirst<event_intf::UpdateSuccessful>("UpdateSuccessful",
                                                      imageIdentifier);
}

void Events::generateVerificationFailed(const std::string& imageIdentifier)
{
    generateImageFirst<error_intf::VerificationFailed>("VerificationFailed",
                                                       imageIdentifier);
}

void Events::generateTransferFailed(const std::string& imageIdentifier)
{
    generateImageFirst<error_intf::TransferFailed>("TransferFailed",
                                                   imageIdentifier);
}

void Events::generateActivateFailed(const std::string& imageIdentifier)
{
    generateImageFirst<error_intf::ActivateFailed>("ActivateFailed",
                                                   imageIdentifier);
}

void Events::generateUpdateNotApplicable(const std::string& imageIdentifier)
{
    generateImageFirst<error_intf::UpdateNotApplicable>("UpdateNotApplicable",
                                                        imageIdentifier);
}

} // namespace pldm::fw_update
