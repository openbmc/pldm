#include "operation_session.hpp"

#include "libbej/bej_dictionary.h"

#include "common/instance_id.hpp"
#include "device.hpp"
#include "libbej/bej_decoder_json.hpp"
#include "libbej/bej_encoder_json.hpp"
#include "operation_task.hpp"
#include "utils.hpp"

#include <libpldm/pldm_types.h>

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

constexpr uint32_t maxBufferSize = 64 * 1024;
constexpr uint8_t CONTAINS_REQ_PAYLOAD = 1;

namespace pldm::rde
{

OperationSession::OperationSession(std::shared_ptr<Device> device,
                                   struct OperationInfo oipInfo) :
    device_(std::move(device)), eid_(device_->eid()),
    currentState_(OpState::Idle), oipInfo(oipInfo)
{
    info("RDE: OperationSession created for EID={EID},use_count={COUNT}", "EID",
         static_cast<int>(eid_), "COUNT", device_.use_count());
}

void OperationSession::updateState(OpState newState)
{
    currentState_ = newState;
}

OpState OperationSession::getState() const
{
    return currentState_;
}

void OperationSession::markComplete()
{
    complete = true;
}

bool OperationSession::isComplete() const
{
    return complete;
}

bool OperationSession::addToOperationBytes(std::span<const uint8_t> payload,
                                           bool hasChecksum)
{
    size_t actualPayloadSize = payload.size();
    if (hasChecksum && actualPayloadSize >= sizeof(uint32_t))
    {
        actualPayloadSize -= sizeof(uint32_t);
        // Remove the last byte (checksum)
        responseBuffer.insert(responseBuffer.end(), payload.begin(),
                              payload.begin() + actualPayloadSize);
    }
    else
    {
        responseBuffer.insert(responseBuffer.end(), payload.begin(),
                              payload.end());
    }
    return true;
}

std::string OperationSession::getRootObjectName(const nlohmann::json& json)
{
    if (json.contains("@odata.type"))
    {
        std::string odataType = json["@odata.type"];
        auto hashPos = odataType.find('#');
        auto dotPos = odataType.find('.', hashPos + 1);
        if (hashPos != std::string::npos && dotPos != std::string::npos)
        {
            return odataType.substr(hashPos + 1, dotPos - hashPos - 1);
        }
    }
    return ""; // fallback
}

RedfishPropertyParent* OperationSession::createBejTree(
    const std::string& name, const nlohmann::json& jsonObj)
{
    auto* root = new RedfishPropertyParent();

    if (name.empty())
        bejTreeInitSet(root, nullptr);
    else
        bejTreeInitSet(root, name.c_str());

    for (auto it = jsonObj.begin(); it != jsonObj.end(); ++it)
    {
        const std::string& key = it.key();
        const auto& value = it.value();

        if (value.is_string())
        {
            auto* leaf = new RedfishPropertyLeafString();
            bejTreeAddString(root, leaf, key.c_str(),
                             value.get<std::string>().c_str());
        }
        else if (value.is_number_integer())
        {
            auto* leaf = new RedfishPropertyLeafInt();
            bejTreeAddInteger(root, leaf, key.c_str(), value.get<int64_t>());
        }
        else if (value.is_boolean())
        {
            auto* leaf = new RedfishPropertyLeafBool();
            bejTreeAddBool(root, leaf, key.c_str(), value.get<bool>());
        }
        else if (value.is_array())
        {
            auto* arrayParent = new RedfishPropertyParent();
            bejTreeInitArray(arrayParent, key.c_str());

            for (const auto& elem : value)
            {
                if (elem.is_string())
                {
                    auto* strElem = new RedfishPropertyLeafString();
                    bejTreeAddString(arrayParent, strElem, "",
                                     elem.get<std::string>().c_str());
                }
                else if (elem.is_number_integer())
                {
                    auto* intElem = new RedfishPropertyLeafInt();
                    bejTreeAddInteger(arrayParent, intElem, "",
                                      elem.get<int64_t>());
                }
                else if (elem.is_boolean())
                {
                    auto* boolElem = new RedfishPropertyLeafBool();
                    bejTreeAddBool(arrayParent, boolElem, "", elem.get<bool>());
                }
                else if (elem.is_object())
                {
                    auto* child = createBejTree("", elem);
                    bejTreeLinkChildToParent(arrayParent, child);
                }
                // handle other types if needed
            }

            bejTreeLinkChildToParent(root, arrayParent);
        }
        else if (value.is_object())
        {
            auto* child = createBejTree(key, value);
            bejTreeLinkChildToParent(root, child);
        }
        else
        {
            error("Invalid type detected in BEJ Tree for value:{VAL} ", "VAL",
                  value);
        }
        // handle null or other types if needed
    }

    return root;
}

inline BejDictionaries OperationSession::getDictionaries()
{
    try
    {
        if (device_ == nullptr || device_->getDictionaryManager() == nullptr)
        {
            lg2::error(
                "RDE: DictionaryManager unavailable for resourceId={RID}",
                "RID", currentResourceId_);
            throw std::runtime_error("DictionaryManager unavailable");
        }

        std::span<const uint8_t> schemaDict =
            device_->getDictionaryManager()
                ->get(currentResourceId_, bejMajorSchemaClass)
                ->getDictionaryBytes();

        std::span<const uint8_t> annotDict =
            device_->getDictionaryManager()
                ->getAnnotationDictionary()
                ->getDictionaryBytes();

        info(
            "RDE: Dictionaries retrieved for resourceId={RID}, SchemaSize={SSIZE}, AnnotationSize={ASIZE}",
            "RID", currentResourceId_, "SSIZE", schemaDict.size(), "ASIZE",
            annotDict.size());

        BejDictionaries dictionaries;
        dictionaries.schemaDictionary = schemaDict.data();
        dictionaries.schemaDictionarySize =
            static_cast<uint32_t>(schemaDict.size());
        dictionaries.annotationDictionary = annotDict.data();
        dictionaries.annotationDictionarySize =
            static_cast<uint32_t>(annotDict.size());
        dictionaries.errorDictionary = nullptr;
        dictionaries.errorDictionarySize = 0;

        return dictionaries;
    }
    catch (const std::exception& ex)
    {
        lg2::error(
            "RDE: Failed to retrieve BEJ dictionaries for resourceId={RID}, Exception={ERR}",
            "RID", currentResourceId_, "ERR", ex.what());
        throw;
    }
}

std::vector<uint8_t> OperationSession::getBejPayload()
{
    BejDictionaries dictionaries = getDictionaries();

    libbej::BejEncoderJson encoder;
    int rc = encoder.encode(
        &dictionaries, bejMajorSchemaClass,
        createBejTree(getRootObjectName(jsonPayload), jsonPayload));
    if (rc != 0)
    {
        error("Failed to encode requestPayload from JSON to BEJ: rc:{RC}", "RC",
              rc);
        return {};
    }

    return encoder.getOutput();
}

std::string OperationSession::getJsonStrPayload()
{
    BejDictionaries dictionaries = getDictionaries();

    libbej::BejDecoderJson decoder;
    int rc =
        decoder.decode(dictionaries, std::span<const uint8_t>(responseBuffer));
    if (rc != 0)
    {
        error("Failed to decode responsePayload from BEJ to JSON: rc:{RC}",
              "RC", rc);
        updateState(OpState::OperationFailed);
        return {};
    }

    return decoder.getOutput();
}

void OperationSession::doOperationInit()
{
    auto instanceId = device_->getInstanceIdDb().next(eid_);

    int rc = 0;
    const std::string& resourceIdStr =
        device_->getRegistry()->getResourceIdFromUri(oipInfo.targetURI);
    currentResourceId_ =
        static_cast<uint32_t>(std::stoul(resourceIdStr));
    rde_op_id operationID = oipInfo.operationID;
    uint32_t sendDataTransferHandle;
    uint8_t operationLocatorLength;
    uint32_t requestPayloadLength;
    bitfield8_t operationFlags = {0};
    std::vector<uint8_t> operationLocator{0};
    std::vector<uint8_t> requestPayload{0};

    info("RDE: Starting operation setup for Resource={RID}, OperationID={OID}",
         "RID", currentResourceId_, "OID", static_cast<uint8_t>(operationID));

    if (oipInfo.operationType == OperationType::READ)
    {
        sendDataTransferHandle = 0;
        operationLocatorLength = 0;
        requestPayloadLength = 0;
    }
    else if (oipInfo.operationType == OperationType::UPDATE)
    {
        operationLocatorLength = 0;
        operationFlags.bits.bit1 = CONTAINS_REQ_PAYLOAD;

        try
        {
            jsonPayload = nlohmann::json::parse(oipInfo.payload);
        }
        catch (const nlohmann::json::parse_error& e)
        {
            error(
                "JSON parsing error: EID '{EID}', targetURI '{URI}, json payload '{PAYLOAD}",
                "EID", oipInfo.eid, "URI", oipInfo.targetURI, "PAYLOAD",
                oipInfo.payload);
            updateState(OpState::OperationFailed);
            device_->getInstanceIdDb().free(eid_, instanceId);
            return;
        }

        payloadBuffer = getBejPayload();

        const auto& chunkMeta =
            device_->getMetadataField("mcMaxTransferChunkSizeBytes");
        const auto* maxChunkSizePtr = std::get_if<uint32_t>(&chunkMeta);
        if (!maxChunkSizePtr)
        {
            error(
                "RDE:Invalid metadata: 'mcMaxTransferChunkSizeBytes' is missing or malformed. EID={EID}",
                "EID", eid_);
            updateState(OpState::OperationFailed);
            device_->getInstanceIdDb().free(eid_, instanceId);
            return;
        }

        uint32_t mcMaxChunkSize = *maxChunkSizePtr;
        uint32_t maxChunkSize =
            (mcMaxChunkSize -
             (sizeof(pldm_msg_hdr) + PLDM_RDE_OPERATION_INIT_REQ_FIXED_BYTES +
              operationLocatorLength));
        if (payloadBuffer.size() > maxChunkSize)
        {
            multiPartTransferFlag = true;
            sendDataTransferHandle = instanceId;
            requestPayloadLength = 0;
        }
        else
        {
            sendDataTransferHandle = 0;
            requestPayloadLength =
                (sizeof(pldm_msg_hdr) +
                 PLDM_RDE_OPERATION_INIT_REQ_FIXED_BYTES +
                 operationLocatorLength + requestPayloadLength);
            requestPayload = payloadBuffer;
            requestPayloadLength = requestPayload.size();
        }
    }

    Request request(
        sizeof(pldm_msg_hdr) + PLDM_RDE_OPERATION_INIT_REQ_FIXED_BYTES +
        operationLocatorLength + requestPayloadLength);
    auto requestMsg = new (request.data()) pldm_msg;

    rc = encode_rde_operation_init_req(
        instanceId, currentResourceId_, operationID,
        static_cast<uint8_t>(oipInfo.operationType), &operationFlags,
        sendDataTransferHandle, operationLocatorLength, requestPayloadLength,
        operationLocator.data(), requestPayload.data(), requestMsg);
    if (rc != PLDM_SUCCESS)
    {
        error("encode OperationInit request EID '{EID}', RC '{RC}'", "EID",
              eid_, "RC", rc);

        updateState(OpState::OperationFailed);
        device_->getInstanceIdDb().free(eid_, instanceId);
        return;
    }

    rc = device_->getHandler()->registerRequest(
        eid_, instanceId, PLDM_RDE, PLDM_RDE_OPERATION_INIT, std::move(request),
        [this](uint8_t /*eid*/, const pldm_msg* respMsg, size_t rxLen) {
            this->handleOperationInitResp(respMsg, rxLen);
        });

    if (rc)
    {
        error("Failed to send request OperationInit EID '{EID}', RC '{RC}'",
              "EID", eid_, "RC", rc);
        device_->getInstanceIdDb().free(eid_, instanceId);
        throw std::runtime_error("Failed to send request OperationInit");
    }

    info("OperationInit Request Waiting");
}

void OperationSession::handleOperationInitResp(const pldm_msg* respMsg,
                                               size_t rxLen)
{
    if (!device_)
    {
        error("RDE:handleOperationInitResp received null device pointer!");
        return;
    }

    if (currentState_ == OpState::TimedOut ||
        currentState_ == OpState::Cancelled)
    {
        info("Late response received. Ignoring callback safely. EID={EID}",
             "EID", eid_);
        return;
    }

    info("handleOperationInitResp Start: EID={EID} rxLen={RXLEN}", "EID",
         device_->eid(), "RXLEN", rxLen);

    if (respMsg == nullptr)
    {
        error("Null PLDM response received from endpoint ID {EID}", "EID",
              eid_);
        updateState(OpState::OperationFailed);
        return;
    }

    if (rxLen == 0)
    {
        error("rxLen is 0; applying fallback length. EID={EID}", "EID", eid_);
        updateState(OpState::OperationFailed);
        return;
    }

    const std::string& resourceIdStr =
        device_->getRegistry()->getResourceIdFromUri(oipInfo.targetURI);
    currentResourceId_ = static_cast<uint32_t>(std::stoul(resourceIdStr));
    uint8_t cc = 0;
    uint8_t operationStatus;
    uint8_t completionPercentage;
    uint32_t completionTimeSeconds;
    bitfield8_t operationExecutionFlags;
    uint32_t resultTransferHandle;
    bitfield8_t permissionFlags;
    uint32_t responsePayloadLength;
    const uint32_t etagMaxSize = 1024;

    uint8_t buffer[sizeof(struct pldm_rde_varstring) + etagMaxSize];
    pldm_rde_varstring* etag = (struct pldm_rde_varstring*)buffer;

    uint8_t responsePayload[maxBufferSize];

    auto rc = decode_rde_operation_init_resp(
        respMsg, rxLen, &cc, &operationStatus, &completionPercentage,
        &completionTimeSeconds, &operationExecutionFlags, &resultTransferHandle,
        &permissionFlags, &responsePayloadLength, etag, responsePayload);

    info(
        "Decoded fields: CC= {CC}, operationStatus={STATUS}, completionPercentage={PCT}, \
        completionTimeSeconds={TIME}, resultTransferHandle={HANDLE}, responsePayloadLength={LENGTH}, etagMaxSize={ETAG}",
        "CC", cc, "STATUS", operationStatus, "PCT", completionPercentage,
        "TIME", completionTimeSeconds, "HANDLE", resultTransferHandle, "LENGTH",
        responsePayloadLength, "ETAG", etagMaxSize);

    if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
    {
        error(
            "Failed to decode handleOperationInitResp response rc:{RC} cc:{CC}",
            "RC", rc, "CC", cc);
        logCompletionCodeError(cc);
        updateState(OpState::OperationFailed);
        emitTaskUpdatedSignal(device_->getBus(), oipInfo.opTaskPath, "{}",
                              static_cast<uint16_t>(OpState::OperationFailed));
        return;
    }

    if (oipInfo.operationType == OperationType::READ)
    {
        info("RDE: Received transferHandle={HANDLE} for resourceId={RID}",
             "HANDLE", resultTransferHandle, "RID", currentResourceId_);

        if (resultTransferHandle == 0)
        {
            addChunk(currentResourceId_,
                     {responsePayload, responsePayloadLength}, false, true);
            std::string decoded = getJsonStrPayload();
            info("Response{STR}", "STR", decoded.c_str());
            emitTaskUpdatedSignal(
                device_->getBus(), oipInfo.opTaskPath, decoded.c_str(),
                static_cast<uint16_t>(OpState::OperationCompleted));
            doOperationComplete();
        }
        else if (resultTransferHandle != 0)
        {
            try
            {
                receiver_ = std::make_unique<pldm::rde::MultipartReceiver>(
                    device_, eid_, resultTransferHandle);

                receiver_->start(
                    [this](std::span<const uint8_t> payload,
                           const pldm::rde::MultipartRcvMeta& meta) {
                        addChunk(currentResourceId_, payload, meta.hasChecksum,
                                 meta.isFinalChunk);
                        if (isComplete())
                        {
                            info("MultipartReceive sequence completed");
                            std::string decoded = getJsonStrPayload();
                            info("Response{STR}", "STR", decoded.c_str());
                            emitTaskUpdatedSignal(
                                device_->getBus(), oipInfo.opTaskPath,
                                decoded.c_str(),
                                static_cast<uint16_t>(
                                    OpState::OperationCompleted));
                            doOperationComplete();
                        }
                        else
                        {
                            receiver_->setTransferOperation(
                                PLDM_RDE_XFER_NEXT_PART);
                            receiver_->sendReceiveRequest(meta.nextHandle);
                        }
                    },
                    [this]() {
                        info(
                            "RDE: Multipart transfer complete for resourceId={RID}",
                            "RID", currentResourceId_);
                    },
                    [this](const std::string& reason) {
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
    }
    else if (oipInfo.operationType == OperationType::UPDATE)
    {
        // TODO: Add handler for RDEMultipartSend
        if (multiPartTransferFlag)
        {
            try
            {
                info(
                    "RDE: Received transferHandle={HANDLE} for resourceId={RID}",
                    "HANDLE", resultTransferHandle, "RID", currentResourceId_);

                sender_ = std::make_unique<pldm::rde::MultipartSender>(
                    device_, eid_, payloadBuffer);

                sender_->start(
                    [this](const pldm::rde::MultipartSndMeta& meta) {
                        if ((meta.isOpComplete))
                        {
                            info("Multipartsend completed");
                            multiPartTransferFlag = false;
                            doOperationComplete();
                        }
                        else
                        {
                            sender_->setTransferFlag(PLDM_RDE_START);
                            sender_->setOperationID(oipInfo.operationID);
                            sender_->sendReceiveRequest(meta.nextHandle);
                        }
                    },
                    [this]() {
                        info(
                            "RDE: Multipart transfer complete for resourceId={RID}",
                            "RID", currentResourceId_);
                    },
                    [this](const std::string& reason) {
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
    }

    return;
}

void OperationSession::doOperationComplete()
{
    auto instanceId = device_->getInstanceIdDb().next(eid_);

    ResourceRegistry* resourceRegistry = device_->getRegistry();
    const std::string& resourceIdStr =
        resourceRegistry->getResourceIdFromUri(oipInfo.targetURI);
    currentResourceId_ = static_cast<uint32_t>(std::stoul(resourceIdStr));

    operationID = oipInfo.operationID;

    Request request(
        sizeof(pldm_msg_hdr) + PLDM_RDE_OPERATION_COMPLETE_REQ_BYTES);
    auto requestMsg = new (request.data()) pldm_msg;

    int rc = encode_rde_operation_complete_req(instanceId, currentResourceId_,
                                               operationID, requestMsg);
    if (rc != PLDM_SUCCESS)
    {
        error("encode OperationComplete request EID '{EID}', RC '{RC}'", "EID",
              eid_, "RC", rc);

        updateState(OpState::OperationFailed);
        device_->getInstanceIdDb().free(eid_, instanceId);
        return;
    }

    rc = device_->getHandler()->registerRequest(
        eid_, instanceId, PLDM_RDE, PLDM_RDE_OPERATION_COMPLETE,
        std::move(request),
        [this](uint8_t /*eid*/, const pldm_msg* respMsg, size_t rxLen) {
            this->handleOperationCompleteResp(respMsg, rxLen);
        });

    if (rc)
    {
        error(
            "Failed to send request OperationCompelete EID '{EID}', RC '{RC}'",
            "EID", eid_, "RC", rc);

        device_->getInstanceIdDb().free(eid_, instanceId);

        throw std::runtime_error("Failed to send request OperationComplete");
    }
}

void OperationSession::handleOperationCompleteResp(const pldm_msg* respMsg,
                                                   size_t rxLen)
{
    info("handleOperationCompleteResp");
    if (currentState_ == OpState::TimedOut ||
        currentState_ == OpState::Cancelled)
    {
        info("Late response received. Ignoring callback safely. EID={EID}",
             "EID", eid_);
        return;
    }

    info("handleOperationCompleteResp Start: EID={EID} rxLen={RXLEN}", "EID",
         eid_, "RXLEN", rxLen);

    if (respMsg == nullptr)
    {
        error("Null PLDM response received from endpoint ID {EID}", "EID",
              eid_);
        updateState(OpState::OperationFailed);
        return;
    }

    if (rxLen == 0)
    {
        error("rxLen is 0; applying fallback length. EID={EID}", "EID", eid_);
        updateState(OpState::OperationFailed);
        return;
    }

    uint8_t cc;
    auto rc = decode_rde_operation_complete_resp(respMsg, rxLen, &cc);
    if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
    {
        error(
            "Failed to decode handleOperationCompleteResp response rc:{RC} cc:{CC}",
            "RC", rc, "CC", cc);
        updateState(OpState::OperationFailed);
        return;
    }

    info("OperationComplete done");
}

void OperationSession::addChunk(uint32_t resourceId,
                                std::span<const uint8_t> payload,
                                bool hasChecksum, bool isFinalChunk)
{
    info("RDE: OperationSession addChunk Enter for resourceID={RC}", "RC",
         resourceId);

    if (payload.empty())
    {
        throw std::invalid_argument("Payload chunk is empty.");
    }

    if (!addToOperationBytes(payload, hasChecksum))
    {
        throw std::runtime_error("Failed to add chunk to dictionary.");
    }

    if (isFinalChunk)
        markComplete();

    info("RDE: OperationSession addChunk Exit for resourceID={RC}", "RC",
         resourceId);
}

} // namespace pldm::rde
