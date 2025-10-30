#include "firmware_inventory.hpp"

namespace pldm::fw_update
{

FirmwareInventory::FirmwareInventory(
    SoftwareIdentifier softwareIdentifier, const std::string& softwarePath,
    const std::string& softwareHash, const std::string& softwareVersion,
    const std::string& associatedEndpoint, SoftwareVersionPurpose purpose) :
    softwareIdentifier(softwareIdentifier),
    softwarePath(std::format("{}_{}", softwarePath, softwareHash)),
    association(this->bus, this->softwarePath.c_str()),
    version(this->bus, this->softwarePath.c_str(),
            SoftwareVersion::action::defer_emit),
    activation(this->bus, this->softwarePath.c_str(),
               SoftwareActivation::action::defer_emit)
{
    this->association.associations(
        {{"running", "ran_on", associatedEndpoint.c_str()}});
    this->version.version(softwareVersion.c_str());
    this->version.purpose(purpose);
    this->version.emit_added();
}

} // namespace pldm::fw_update
