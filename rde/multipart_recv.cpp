#include "multipart_recv.hpp"

#include "device.hpp"
#include "device_common.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cstring>

PHOSPHOR_LOG2_USING;

namespace pldm::rde
{

MultipartReceiver::MultipartReceiver(std::shared_ptr<Device> device,
                                     uint8_t eid, uint32_t transferHandle) :
    device_(std::move(device)), eid_(eid), transferHandle_(transferHandle)
{}

void MultipartReceiver::start(
    std::function<void(std::span<const uint8_t>, const MultipartRcvMeta&)>
        onData,
    std::function<void()> onComplete,
    std::function<void(std::string)> onFailure)
{
    onData_ = std::move(onData);
    onComplete_ = std::move(onComplete);
    onFailure_ = std::move(onFailure);

    sendReceiveRequest(transferHandle_);
}

void MultipartReceiver::sendReceiveRequest(uint32_t handle)
{
    uint8_t instanceId = device_->getInstanceIdDb().next(eid_);

    lg2::debug("RDE: Allocated Instance ID={ID} for EID={EID}", "ID",
               instanceId, "EID", eid_);

    Request request(
        sizeof(pldm_msg_hdr) + PLDM_RDE_MULTIPART_RECEIVE_REQ_BYTES);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());

    rde_op_id operationID{};

    lg2::debug(
        "RDE: Sending multipart request: EID={EID}, HANDLE={HANDLE}, INST_ID={ID}, OP={OP}",
        "EID", eid_, "HANDLE", handle, "ID", instanceId, "OP",
        transferOperation_);

    int rc = encode_rde_multipart_receive_req(instanceId, handle, operationID,
                                              transferOperation_, requestMsg);
    if (rc != PLDM_SUCCESS)
    {
        device_->getInstanceIdDb().free(eid_, instanceId);
        lg2::error("RDE: Request encoding failed: RC={RC}, EID={EID}", "RC", rc,
                   "EID", eid_);
        if (onFailure_)
            onFailure_("RDE: Request encoding failed");
        return;
    }

    rc = device_->getHandler()->registerRequest(
        eid_, instanceId, PLDM_RDE, PLDM_RDE_MULTIPART_RECEIVE,
        std::move(request),
        [this](uint8_t /*eid*/, const pldm_msg* msg, size_t len) {
            this->handleReceiveResp(msg, len);
        });

    if (rc)
    {
        device_->getInstanceIdDb().free(eid_, instanceId);
        lg2::error("RDE: Request registration failed: RC={RC}, EID={EID}", "RC",
                   rc, "EID", eid_);
        if (onFailure_)
            onFailure_("RDE: Request registration failed");
    }
}

void MultipartReceiver::handleReceiveResp(const pldm_msg* respMsg, size_t rxLen)
{
    lg2::info("RDE: :handleReceiveResp LEN={LEN}", "LEN", rxLen);

    if (!respMsg || rxLen == 0)
    {
        lg2::error("RDE: Empty multipart response: EID={EID}, LEN={LEN}", "EID",
                   eid_, "LEN", rxLen);
        if (onFailure_)
            onFailure_("RDE: Empty or invalid response");
        return;
    }

    constexpr size_t kFixedHeaderBytes =
        PLDM_RDE_MULTIPART_RECEIVE_RESP_FIXED_BYTES;
    if (rxLen <= kFixedHeaderBytes)
    {
        lg2::error(
            "RDE: Response too short to contain payload: EID={EID}, LEN={LEN}",
            "EID", eid_, "LEN", rxLen);
        if (onFailure_)
            onFailure_("RDE: Response length too small");
        return;
    }

    const auto& chunkMeta =
        device_->getMetadataField("devMaxTransferChunkSizeBytes");
    const auto* chunkSizePtr = std::get_if<uint32_t>(&chunkMeta);
    if (!chunkSizePtr)
    {
        error(
            "RDE: Invalid metadata: 'mcMaxTransferChunkSizeBytes' is missing or malformed. EID={EID}",
            "EID", eid_);
        return;
    }

    const uint32_t declaredMaxSize = *chunkSizePtr;
    const size_t payloadBytes =
        rxLen - PLDM_RDE_MULTIPART_RECEIVE_RESP_FIXED_BYTES;
    std::vector<uint8_t> buffer(payloadBytes, 0);

    uint8_t cc = 0, transferFlag = 0;
    uint32_t nextHandle = 0, length = 0, checksum = 0;

    int rc = decode_rde_multipart_receive_resp(
        respMsg, rxLen, &cc, &transferFlag, &nextHandle, &length, buffer.data(),
        &checksum);

    if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
    {
        lg2::error("RDE: Chunk decode failed: RC={RC}, CC={CC}, EID={EID}",
                   "RC", rc, "CC", cc, "EID", eid_);
        if (onFailure_)
            onFailure_("Chunk decode failed");
        return;
    }

    if (length > buffer.size())
    {
        lg2::error(
            "RDE: Declared chunk length overflow: LEN={LEN}, ALLOCATED={ALLOC}, EID={EID}",
            "LEN", length, "ALLOC", buffer.size(), "EID", eid_);
        if (onFailure_)
            onFailure_("RDE: Declared chunk length exceeds allocated buffer");
        return;
    }

    std::span<const uint8_t> payload(buffer.data(), length);
    const bool isFinalChunk = (nextHandle == 0);

    MultipartRcvMeta meta{.schemaClass = 0,
                          .hasChecksum = isFinalChunk,
                          .isFinalChunk = isFinalChunk,
                          .nextHandle = nextHandle,
                          .checksum = checksum,
                          .length = length};

    // Logging decoded metadata (excluding payload)
    lg2::debug(
        "RDE: Multipart chunk decoded successfully: EID={EID}, CC={CC}, TF={TF},\
        HANDLE={HANDLE}, LEN={LEN}, CHECKSUM={CHK}, FINAL={FINAL}, MAX_META={MAX}",
        "EID", eid_, "CC", cc, "TF", transferFlag, "HANDLE", nextHandle, "LEN",
        length, "CHK", checksum, "FINAL", isFinalChunk, "MAX", declaredMaxSize);

    if (onData_)
    {
        onData_(payload, meta);
    }
}

} // namespace pldm::rde
