#include "device_dedicated_updater.hpp"

#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <string>

using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

DeviceDedicatedUpdater::DeviceDedicatedUpdater(
    Context& ctx, Event& /*event*/,
    pldm::requester::Handler<pldm::requester::Request>& /*handler*/,
    InstanceIdDb& /*instanceIdDb*/, pldm::eid eid,
    const std::string& softwarePath, const std::string& softwareVersion,
    const std::string& associatedEndpoint, const Descriptors& descriptors,
    const ComponentInfo& componentInfo, SoftwareVersionPurpose purpose) :
    ctx(ctx), eid(eid), softwarePath(softwarePath),
    activation(
        std::make_unique<SoftwareActivation>(ctx, this->softwarePath.c_str())),
    association(std::make_unique<SoftwareAssociationDefinitions>(
        ctx, this->softwarePath.c_str())),
    version(std::make_unique<SoftwareVersion>(ctx, this->softwarePath.c_str())),
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

Response DeviceDedicatedUpdater::handleRequest(
    Command /*command*/, const pldm_msg* request, size_t /*reqMsgLen*/)
{
    auto response = Response{sizeof(pldm_msg_hdr), 0};
    auto ptr = new (response.data()) pldm_msg;
    auto rc = encode_cc_only_resp(request->hdr.instance_id, request->hdr.type,
                                  request->hdr.command,
                                  PLDM_FWUP_COMMAND_NOT_EXPECTED, ptr);
    assert(rc == PLDM_SUCCESS);
    return response;
}

} // namespace pldm::fw_update
