#include "multipart_send.hpp"

#include "device.hpp"
#include "device_common.hpp"

namespace pldm::rde
{

MultipartSender::MultipartSender(std::shared_ptr<Device> device, uint8_t eid,
                                 std::vector<uint8_t> dataPayload) :
    device_(std::move(device)), eid_(eid), dataPayload_(dataPayload)
{}

void MultipartSender::start(std::function<void(const MultipartSndMeta&)> onData,
                            std::function<void()> onComplete,
                            std::function<void(std::string)> onFailure)
{
    onData_ = std::move(onData);
    onComplete_ = std::move(onComplete);
    onFailure_ = std::move(onFailure);

    sendRequest(transferHandle_);
}

void MultipartSender::setOperationID(rde_op_id op)
{
    operationID_ = op;
}

void MultipartSender::setTransferFlag(uint8_t flag)
{
    transferFlag_ = flag;
}

void MultipartSender::sendRequest(uint32_t handle)
{
    const auto& meta = device_->getMetadataField("mcMaxTransferChunkSizeBytes");
    const auto* maxChunkSizePtr = std::get_if<uint32_t>(&meta);
    if (!maxChunkSizePtr)
    {
        if (onFailure_)
            onFailure_(
                "Missing or invalid mcMaxTransferChunkSizeBytes metadata");
        return;
    }

    uint32_t maxChunkSize = *maxChunkSizePtr;
    std::vector<uint8_t> first_chunk;
    first_chunk = std::vector<uint8_t>(dataPayload_.begin(),
                                       dataPayload_.begin() + maxChunkSize);

    sendMultipartCommand(handle, operationID_, PLDM_RDE_START, first_chunk, 0);
}

void MultipartSender::sendReceiveRequest(uint32_t handle)
{
    const auto& meta = device_->getMetadataField("mcMaxTransferChunkSizeBytes");
    const auto* maxChunkSizePtr = std::get_if<uint32_t>(&meta);
    if (!maxChunkSizePtr)
    {
        if (onFailure_)
            onFailure_(
                "Missing or invalid mcMaxTransferChunkSizeBytes metadata");
        return;
    }

    uint32_t maxChunkSize = *maxChunkSizePtr;
    std::vector<uint8_t> chunk;

    if (dataPayload_.size() <= maxChunkSize)
    {
        chunk = dataPayload_;
        transferFlag_ = PLDM_RDE_END;
        handle = 0;
    }
    else
    {
        chunk = std::vector<uint8_t>(dataPayload_.begin(),
                                     dataPayload_.begin() + maxChunkSize);
        transferFlag_ = PLDM_RDE_MIDDLE;
    }

    uint32_t checksum = 0; // Optional: implement checksum logic if required

    sendMultipartCommand(handle, operationID_, transferFlag_, chunk, checksum);
}

bool MultipartSender::sendMultipartCommand(
    uint32_t handle, rde_op_id operationID, uint8_t transferFlag,
    const std::vector<uint8_t>& payload, uint32_t checksum)
{
    uint8_t instanceId = device_->getInstanceIdDb().next(eid_);

    uint32_t dataLength = payload.size();
    Request request(sizeof(pldm_msg_hdr) +
                    PLDM_RDE_MULTIPART_SEND_REQ_FIXED_BYTES + dataLength);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());

    // Use 'handle' twice per spec:
    // - As DataTransferHandle for current chunk
    // - Also as NextDataTransferHandle when sending
    // (actual next handle is returned by device)
    // Behavior varies based on operation flag (e.g. FIRST_PART vs NEXT_PART)

    int rc = encode_rde_multipart_send_req(
        instanceId, handle, operationID, transferFlag, handle, dataLength,
        const_cast<uint8_t*>(payload.data()), checksum, requestMsg);
    if (rc != PLDM_SUCCESS)
    {
        device_->getInstanceIdDb().free(eid_, instanceId);
        if (onFailure_)
            onFailure_("Request encoding failed");
        return false;
    }

    rc = device_->getHandler()->registerRequest(
        eid_, instanceId, PLDM_RDE, PLDM_RDE_MULTIPART_SEND, std::move(request),
        [this](uint8_t, const pldm_msg* msg, size_t len) {
            this->handleSendResp(msg, len);
        });

    if (rc)
    {
        device_->getInstanceIdDb().free(eid_, instanceId);
        if (onFailure_)
            onFailure_("Request registration failed");
        return false;
    }

    return true;
}

void MultipartSender::handleSendResp(const pldm_msg* respMsg, size_t rxLen)
{
    uint8_t cc = 0, transferOperation = 0;

    int rc =
        decode_rde_multipart_send_resp(respMsg, rxLen, &cc, &transferOperation);
    if (rc != PLDM_SUCCESS)
    {
        if (onFailure_)
            onFailure_("Response decode failed");
        return;
    }

    if (cc != PLDM_SUCCESS)
    {
        if (onFailure_)
            onFailure_(
                "PLDM command failed: CompletionCode=" + std::to_string(cc));
        return;
    }

    const bool isOpComplete = (transferOperation == PLDM_XFER_COMPLETE);
    MultipartSndMeta meta{
        .schemaClass = 0, // can be extended later
        .hasChecksum = true,
        .isOpComplete = isOpComplete,
        .nextHandle = (transferHandle_ += 1),
    };
    if (onData_)
    {
        onData_(meta);
    }

    if (meta.isOpComplete)
    {
        if (onComplete_)
            onComplete_();
    }
    else
    {
        transferHandle_ += 1;
        sendReceiveRequest(transferHandle_);
    }
}

} // namespace pldm::rde
