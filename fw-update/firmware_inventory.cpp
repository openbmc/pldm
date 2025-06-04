#include "firmware_inventory.hpp"

namespace pldm::fw_update
{

FirmwareInventory::FirmwareInventory(
    SoftwareIdentifier softwareIdentifier, const std::string& softwarePath,
    const std::string& softwareVersion, const std::string& associatedEndpoint,
    const Descriptors& descriptors, const ComponentInfo& componentInfo,
    AggregateUpdateManager& updateManager, SoftwareVersionPurpose purpose,
    const ConditionPaths& conditionPaths, const std::string& conditionArg,
    std::function<void()> taskCompletionCallback) :
    softwareIdentifier(softwareIdentifier), softwarePath(softwarePath),
    association(this->bus, this->softwarePath.c_str()),
    version(this->bus, this->softwarePath.c_str(),
            SoftwareVersion::action::defer_emit),
    updateManager(updateManager)
{
    updateManager.createUpdateManager(
        softwareIdentifier, descriptors, componentInfo, softwarePath,
        conditionPaths, conditionArg, std::move(taskCompletionCallback));
    this->association.associations(
        {{"running", "ran_on", associatedEndpoint.c_str()}});
    this->version.version(softwareVersion.c_str());
    this->version.purpose(purpose);
    this->version.emit_added();
}

FirmwareInventory::~FirmwareInventory()
{
    updateManager.eraseUpdateManager(softwareIdentifier);
}

} // namespace pldm::fw_update
