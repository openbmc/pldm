#include "device_dedicated_updater.hpp"

#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <string>

using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

PHOSPHOR_LOG2_USING;

namespace pldm
{

namespace fw_update
{

DeviceDedicatedUpdater::DeviceDedicatedUpdater(
    Context& ctx, pldm::eid eid, const std::string& softwarePath,
    const std::string& softwareVersion, const std::string& associatedEndpoint,
    const Descriptors& descriptors, const ComponentInfo& componentInfo,
    SoftwareVersionPurpose purpose) :
    ctx(ctx), eid(eid), softwarePath(softwarePath),
    activation(std::make_unique<SoftwareActivation>(ctx, softwarePath.c_str())),
    association(std::make_unique<SoftwareAssociationDefinitions>(
        ctx, softwarePath.c_str())),
    version(std::make_unique<SoftwareVersion>(ctx, softwarePath.c_str())),
    updateInterface(
        std::make_unique<SoftwareUpdate>(ctx, softwarePath.c_str(), *this)),
    descriptors(descriptors), componentInfo(componentInfo)
{
    this->association->associations(
        {{"running", "ran_on", associatedEndpoint.c_str()}});
    this->activation->activation(SoftwareActivation::Activations::Active);
    this->activation->emit_added();
    this->version->version(softwareVersion.c_str());
    this->version->purpose(purpose);
    this->version->emit_added();
}

} // namespace fw_update
} // namespace pldm
