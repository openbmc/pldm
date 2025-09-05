#include "discov_session.hpp"

#include "common/instance_id.hpp"
#include "device.hpp"
#include "dictionary_manager.hpp"
#include "requester/handler.hpp"

extern "C"
{
#include <libpldm/base.h>
#include <libpldm/platform.h>
#include <libpldm/pldm_types.h>
#include <libpldm/rde.h>
}
#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm::rde
{
DiscoverySession::DiscoverySession(std::shared_ptr<Device> device) :
    device_(std::move(device)), eid_(device_->eid()), tid_(device_->getTid()),
    currentState_(OpState::Idle)
{
    info(
        "RDE: DiscoverySession created for EID={EID},TID={TID}, use_count={COUNT}",
        "EID", static_cast<int>(eid_), "TID", tid_, "COUNT",
        device_.use_count());

    // Optional: log pointers if debugging deep ownership
    if (!device_)
    {
        error("RDE:DiscoverySession received null device pointer!");
    }

    // Optional: print address for deep tracking
    debug("RDE:DiscoverySession bound to Device instance at address={ADDR}",
          "ADDR", static_cast<const void*>(device_.get()));
}

void DiscoverySession::updateState(OpState newState)
{
    currentState_ = newState;
}

OpState DiscoverySession::getState() const
{
    return currentState_;
}

void DiscoverySession::sendRDECommand(
    uint8_t eid, uint8_t instanceId, uint8_t command, Request&& request,
    std::function<void(const pldm_msg*, size_t)> callback)
{
    info("RDE: sendRDECommand Start: EID={EID} InstanceID={IID}, Command={CMD}",
         "EID", eid, "IID", instanceId, "CMD", command);

    int rc = device_->getHandler()->registerRequest(
        eid, instanceId, PLDM_RDE, command, std::move(request),
        [callback,
         device = device_](uint8_t /*eid*/, const pldm_msg* respMsg,
                           size_t rxLen) { callback(respMsg, rxLen); });
    if (rc != PLDM_SUCCESS)
    {
        error("RDE: Failed to send command {CMD} to EID={EID}, RC={RC}", "CMD",
              command, "EID", eid, "RC", rc);
        device_->getInstanceIdDb().free(eid, instanceId);
        return;
    }

    info("RDE: Command {CMD} sent, awaiting response…", "CMD", command);
}

void DiscoverySession::doNegotiateRedfish()
{
    info("RDE: NegotiateRedfishParameters  Enter");

    auto instanceId = device_->getInstanceIdDb().next(eid_);

    Request request(
        sizeof(pldm_msg_hdr) + PLDM_RDE_NEGOTIATE_REDFISH_PARAMETERS_REQ_BYTES);
    auto requestMsg = new (request.data()) pldm_msg;

    // Variant-safe: Extract mcFeatureSupport
    const auto& featureMeta = device_->getMetadataField("mcFeatureSupport");
    const auto* featureSupport = std::get_if<FeatureSupport>(&featureMeta);
    if (!featureSupport)
    {
        error(
            "RDE: Invalid metadata type: 'mcFeatureSupport' is missing or malformed. EID={EID}",
            "EID", eid_);
        return;
    }

    bitfield16_t mcFeatureBits;
    mcFeatureBits.value = featureSupport->value;

    // Variant-safe: Extract mcConcurrencySupport
    const auto& concurrencyMeta =
        device_->getMetadataField("mcConcurrencySupport");
    const auto* concurrencySupport = std::get_if<uint8_t>(&concurrencyMeta);
    if (!concurrencySupport)
    {
        error(
            "RDE:Invalid metadata type: 'mcConcurrencySupport' is missing or malformed. EID={EID}",
            "EID", eid_);
        return;
    }

    info(
        "RDE: NegotiateRedfishParameters EID={EID}, InstID={IID},ConSupportt={CONC}, MCFeature={FEAT}",
        "EID", eid_, "IID", instanceId, "CONC", *concurrencySupport, "FEAT",
        featureSupport->value);

    int rc = encode_negotiate_redfish_parameters_req(
        instanceId, *concurrencySupport, &mcFeatureBits, requestMsg);

    if (rc != PLDM_SUCCESS)
    {
        error(
            "RDE: encode NegotiateRedfishParameters request EID '{EID}', RC '{RC}'",
            "EID", eid_, "RC", rc);
        updateState(OpState::OperationFailed);
        device_->getInstanceIdDb().free(eid_, instanceId);
        return;
    }

    sendRDECommand(eid_, instanceId, PLDM_NEGOTIATE_REDFISH_PARAMETERS,
                   std::move(request),
                   [this](const pldm_msg* respMsg, size_t rxLen) {
                       this->handleNegotiateRedfishResp(respMsg, rxLen);
                   });
}

void DiscoverySession::handleNegotiateRedfishResp(const pldm_msg* respMsg,
                                                  size_t rxLen)
{
    info("RDE: handleNegotiateRedfishResp Start: EID={EID} rxLen={RXLEN}",
         "EID", eid_, "RXLEN", rxLen);

    if (currentState_ == OpState::TimedOut ||
        currentState_ == OpState::Cancelled)
    {
        info("RDE: Late response received. Ignoring callback safely. EID={EID}",
             "EID", eid_);
        return;
    }

    if (respMsg == nullptr)
    {
        error("RDE: Null PLDM response received from endpoint ID {EID}", "EID",
              eid_);
        updateState(OpState::OperationFailed);
        device_->negotiationStatus(device_->NegotiationStatus::Failed, false);
        return;
    }

    if (rxLen == 0)
    {
        error("RDE:rxLen is 0; applying fallback length. EID={EID}", "EID",
              eid_);
        updateState(OpState::OperationFailed);
        device_->negotiationStatus(device_->NegotiationStatus::Failed, false);
        return;
    }

    uint8_t cc = 0;
    uint8_t devConcurrency = 0;
    bitfield8_t devCaps = {};
    bitfield16_t devFeatures = {};
    uint32_t configSig = 0;
    pldm_rde_varstring providerName = {};

    int rc = decode_negotiate_redfish_parameters_resp(
        respMsg, rxLen, &cc, &devConcurrency, &devCaps, &devFeatures,
        &configSig, &providerName);

    if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
    {
        error(
            "RDE: Failed to decode NegotiateRedfishParameters response rc:{RC} cc:{CC}",
            "RC", rc, "CC", cc);
        updateState(OpState::OperationFailed);
        device_->negotiationStatus(device_->NegotiationStatus::Failed, false);
        return;
    }

    FeatureSupport features(devFeatures);
    DeviceCapabilities caps{devCaps};

    info(
        "RDE: NegotiateRedfishParameters response: EID={EID},Signature={SIG}, Provider={PROV} \
        ConcurrencySupport={CONCURRENCY}, FeatureSupport={FEATURE}, DeviceCapabilities={CAPS}",
        "EID", eid_, "SIG", configSig, "PROV", providerName.string_data,
        "CONCURRENCY", devConcurrency, "FEATURE", features.value, "CAPS",
        caps.value);

    if (device_)
    {
        device_->setMetadataField("deviceConcurrencySupport", devConcurrency);
        device_->setMetadataField("devCapabilities", caps);
        device_->setMetadataField("devFeatureSupport", features);
        device_->setMetadataField("devConfigSignature", configSig);
    }

    info("RDE: NegotiateRedfishParameters Command completed");

    // Call Next command NegotiateMedium Parameters
    doNegotiateMediumParams();
}

void DiscoverySession::doNegotiateMediumParams()
{
    info("RDE: NegotiateMediumParameters Enter EID={EID}", "EID", eid_);

    auto instanceId = device_->getInstanceIdDb().next(eid_);

    Request request(
        sizeof(pldm_msg_hdr) + PLDM_RDE_NEGOTIATE_MEDIUM_PARAMETERS_REQ_BYTES);
    auto requestMsg = new (request.data()) pldm_msg;

    const auto& chunkMeta =
        device_->getMetadataField("mcMaxTransferChunkSizeBytes");
    const auto* chunkSizePtr = std::get_if<uint32_t>(&chunkMeta);
    if (!chunkSizePtr)
    {
        error(
            "RDE:Invalid metadata: 'mcMaxTransferChunkSizeBytes' is missing or malformed. EID={EID}",
            "EID", eid_);
        return;
    }

    uint32_t chunkSize = *chunkSizePtr;

    info(
        "RDE: NegotiateMediumParameters request: EID={EID}, InstanceID={IID}, ChunkSize={CHUNK}",
        "EID", eid_, "IID", instanceId, "CHUNK", chunkSize);

    int rc = encode_negotiate_medium_parameters_req(instanceId, chunkSize,
                                                    requestMsg);
    if (rc != PLDM_SUCCESS)
    {
        error(
            "RDE: Encoding NegotiateMediumParameters failed: EID={EID}, RC={RC}",
            "EID", eid_, "RC", rc);
        device_->getInstanceIdDb().free(eid_, instanceId);
        return;
    }

    sendRDECommand(eid_, instanceId, PLDM_NEGOTIATE_MEDIUM_PARAMETERS,
                   std::move(request),
                   [this](const pldm_msg* respMsg, size_t rxLen) {
                       this->handleNegotiateMediumResp(respMsg, rxLen);
                   });
}

void DiscoverySession::handleNegotiateMediumResp(const pldm_msg* respMsg,
                                                 size_t rxLen)
{
    info("RDE: handleNegotiateMediumResp Start: EID={EID} rxLen={RXLEN}", "EID",
         eid_, "RXLEN", rxLen);

    if (currentState_ == OpState::TimedOut ||
        currentState_ == OpState::Cancelled)
    {
        info("RDE: Late response received. Ignoring callback safely. EID={EID}",
             "EID", eid_);
        return;
    }

    if (!respMsg)
    {
        error("RDE: Null PLDM response received from Endpoint ID {EID}", "EID",
              eid_);
        device_->negotiationStatus(device_->NegotiationStatus::Failed, false);
        updateState(OpState::OperationFailed);
        return;
    }

    if (rxLen == 0)
    {
        error("RDE: rxLen is 0; Bad response Packet. EID={EID}", "EID", eid_);
        device_->negotiationStatus(device_->NegotiationStatus::Failed, false);
        updateState(OpState::OperationFailed);
        return;
    }

    uint8_t cc = 0;
    uint32_t devMaxTransferChunkSizeBytes = 0;

    int rc = decode_negotiate_medium_parameters_resp(
        respMsg, rxLen, &cc, &devMaxTransferChunkSizeBytes);

    if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
    {
        error(
            "RDE: Failed to decode NegotiateMediumParameters response rc:{RC} cc:{CC}",
            "RC", rc, "CC", cc);
        device_->negotiationStatus(device_->NegotiationStatus::Failed, false);
        updateState(OpState::OperationFailed);
        return;
    }

    info(
        "RDE: NegotiateMediumParameters response: EID={EID}, DeviceMaxTransfer={SIZE}",
        "EID", eid_, "SIZE", devMaxTransferChunkSizeBytes);

    device_->setMetadataField("devMaxTransferChunkSizeBytes",
                              devMaxTransferChunkSizeBytes);

    info("RDE: NegotiateMediumParameters Command completed");

    // Call Next command GetSchemaDictionary on all avilable resources.
    getDictionaries();
}

void DiscoverySession::getDictionaries()
{
    const std::string triggerFile = "/tmp/.enable_dict_bootstrap";
    if (std::filesystem::exists(triggerFile))
    {
        info("RDE: Skipping dictionary via discovery process");
        return;
    }

    // Collect all applicable resource IDs from device's registry
    const auto& resourceMap = device_->getRegistry()->getResourceMap();

    for (const auto& [rid, info] : resourceMap)
    {
        if (info.schemaClass == PLDM_RDE_SCHEMA_MAJOR)
        {
            majorSchemaResources_.emplace_back(std::stoul(rid),
                                               info.schemaClass);
        }
    }

    resourceIndex_ = 0;

    // Schedule the first dictionary command using a deferred source
    dictionaryDefer_ = std::make_unique<sdeventplus::source::Defer>(
        device_->getEvent(), [this](sdeventplus::source::EventBase&) {
            this->runNextDictionaryCommand(resourceIndex_);
        });
}

void DiscoverySession::runNextDictionaryCommand(size_t index)
{
    dictionaryDefer_.reset();

    if (index >= majorSchemaResources_.size())
    {
        info("RDE: All schema dictionary commands completed.");
        // Update negotiation status
        device_->negotiationStatus(device_->NegotiationStatus::Success, false);
        return;
    }

    const auto& resource = majorSchemaResources_[index];
    uint32_t resourceId = resource.first;
    pldm_rde_schema_type schemaClass = resource.second;

    info("RDE: Discovering dictionary for resourceId={RID}, schema={SCHEMA}",
         "RID", resourceId, "SCHEMA", schemaClass);

    resourceIndex_ = index + 1;

    auto instanceId = device_->getInstanceIdDb().next(eid_);

    size_t payloadLength =
        sizeof(pldm_msg_hdr) + PLDM_RDE_SCHEMA_DICTIONARY_REQ_BYTES;
    Request request(payloadLength);
    auto requestMsg = new (request.data()) pldm_msg;

    int rc = encode_get_schema_dictionary_req(
        instanceId, resourceId, schemaClass, payloadLength, requestMsg);
    if (rc != PLDM_SUCCESS)
    {
        error("RDE: Encoding GetSchemaDictionary failed: RC={RC}", "RC", rc);
        device_->getInstanceIdDb().free(eid_, instanceId);

        dictionaryDefer_ = std::make_unique<sdeventplus::source::Defer>(
            device_->getEvent(), [this](sdeventplus::source::EventBase&) {
                this->runNextDictionaryCommand(resourceIndex_);
            });

        return;
    }

    currentResourceId_ = resourceId;
    currentSchemaClass_ = schemaClass;

    sendRDECommand(eid_, instanceId, PLDM_GET_SCHEMA_DICTIONARY,
                   std::move(request),
                   [this](const pldm_msg* respMsg, size_t rxLen) {
                       handleGetSchemaDictionaryResp(respMsg, rxLen);
                   });
}

void DiscoverySession::handleGetSchemaDictionaryResp(const pldm_msg* respMsg,
                                                     size_t rxLen)
{
    if (!respMsg || rxLen == 0)
    {
        error("RDE: Invalid response for resourceId={RID}", "RID",
              currentResourceId_);

        dictionaryDefer_ = std::make_unique<sdeventplus::source::Defer>(
            device_->getEvent(), [this](sdeventplus::source::EventBase&) {
                this->runNextDictionaryCommand(resourceIndex_);
            });
        return;
    }

    uint8_t cc = 0;
    uint8_t dictionaryFormat = 0;
    uint32_t transferHandle = 0;

    int rc = decode_get_schema_dictionary_resp(
        respMsg, rxLen, &cc, &dictionaryFormat, &transferHandle);

    if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
    {
        error(
            "RDE: Failed to decode response for resourceId={RID}, RC={RC}, CC={CC}",
            "RID", currentResourceId_, "RC", rc, "CC", cc);

        dictionaryDefer_ = std::make_unique<sdeventplus::source::Defer>(
            device_->getEvent(), [this](sdeventplus::source::EventBase&) {
                this->runNextDictionaryCommand(resourceIndex_);
            });
        return;
    }

    try
    {
        info("RDE: Received transferHandle={HANDLE} for resourceId={RID}",
             "HANDLE", transferHandle, "RID", currentResourceId_);

        receiver_ = std::make_unique<pldm::rde::MultipartReceiver>(
            device_, eid_, transferHandle);

        receiver_->start(
            [this](std::span<const uint8_t> payload,
                   const pldm::rde::MultipartRcvMeta& meta) {
                auto* dictMgr = device_->getDictionaryManager();
                if (!dictMgr)
                {
                    error(
                        "RDE: DictionaryManager unavailable for resourceId={RID}",
                        "RID", currentResourceId_);
                    return;
                }

                dictMgr->addChunk(currentResourceId_, meta.schemaClass, payload,
                                  meta.hasChecksum, meta.isFinalChunk);

                const Dictionary* dictionary =
                    dictMgr->get(currentResourceId_, meta.schemaClass);
                if (!dictionary)
                {
                    error(
                        "RDE: No dictionary found for resourceId={RID}, schemaClass={SC}",
                        "RID", currentResourceId_, "SC", meta.schemaClass);
                    return;
                }

                if (dictionary->isComplete())
                {
                    dictionaryDefer_ =
                        std::make_unique<sdeventplus::source::Defer>(
                            device_->getEvent(),
                            [this](sdeventplus::source::EventBase&) {
                                this->runNextDictionaryCommand(resourceIndex_);
                                receiver_.reset();
                            });
                }
                else
                {
                    receiver_->setTransferOperation(PLDM_RDE_XFER_NEXT_PART);
                    receiver_->sendReceiveRequest(meta.nextHandle);
                }
            },
            [this]() {
                info("RDE: Multipart transfer complete for resourceId={RID}",
                     "RID", currentResourceId_);
            },
            [this](const std::string& reason) {
                // TODO: Error handling need to add here.
                error(
                    "RDE: Multipart transfer failed for resourceId={RID}, Error={ERR}",
                    "RID", currentResourceId_, "ERR", reason);
            });
    }
    catch (const std::exception& ex)
    {
        error(
            "RDE: Exception during multipart transfer setup for resourceId={RID}, Error={ERR}",
            "RID", currentResourceId_, "ERR", ex.what());
    }
}

} // namespace pldm::rde
