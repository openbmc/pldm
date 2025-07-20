#pragma once

extern "C"
{
#include <libpldm/base.h>
#include <libpldm/platform.h>
#include <libpldm/pldm_types.h>
#include <libpldm/rde.h>
}

#include <common/instance_id.hpp>
#include <sdeventplus/event.hpp>

#include <functional>
#include <memory>
#include <span>

namespace pldm::rde
{

class Device;

/**
 * @struct MultipartRcvMeta
 * @brief Metadata describing a multipart recive chunk
 */
struct MultipartRcvMeta
{
    uint8_t schemaClass = 0;   // Schema class for this chunk
    bool hasChecksum = true;   // Checksum validity flag
    bool isFinalChunk = false; // Indicates last chunk in transfer
    uint32_t nextHandle = 0;   // Handle for next chunk, if any
    uint32_t checksum = 0;     // Optional checksum value
    uint32_t length = 0;       // Length of payload
};

/**
 * @class MultipartReceiver
 * @brief Handles multipart PLDM RDE dictionary transfer decoding and
 * orchestration.
 *
 * This class decodes multipart responses and emits chunks with associated
 * metadata. Transfer orchestration is delegated to the caller via callbacks.
 */
class MultipartReceiver
{
  public:
    /**
     * @brief Constructor
     *
     * @param device Managed device context
     * @param eid Target device endpoint ID
     * @param transferHandle Initial dictionary transfer handle
     */
    MultipartReceiver(std::shared_ptr<Device> device, uint8_t eid,
                      uint32_t transferHandle);

    /**
     * @brief Starts multipart transfer sequence
     *
     * @param onData Callback triggered for each decoded payload chunk
     * @param onComplete Callback triggered when transfer completes (optional)
     * @param onFailure Callback triggered on error or decode failure (optional)
     */
    void start(
        std::function<void(std::span<const uint8_t>, const MultipartRcvMeta&)>
            onData,
        std::function<void()> onComplete = nullptr,
        std::function<void(std::string)> onFailure = nullptr);

    /**
     * @brief Triggers multipart request to fetch chunk by handle
     *
     * @param handle Transfer handle to request
     */
    void sendReceiveRequest(uint32_t handle);

    /**
     * @brief Handles PLDM multipart response message
     *
     * @param respMsg Pointer to PLDM response message
     * @param rxLen Length of response buffer
     */
    void handleReceiveResp(const pldm_msg* respMsg, size_t rxLen);

    /**
     * @brief Set the transfer operation mode for the next multipart request.
     *
     * This operation updates the internal transferOperation_ value, which
     * controls the PLDM RDE protocol semantics of the next request. Common
     * values include:
     * - PLDM_RDE_XFER_FIRST_PART
     * - PLDM_RDE_XFER_NEXT_PART
     *
     * @param op The operation mode to apply for the next request.
     */
    void setTransferOperation(rde_op_id op)
    {
        transferOperation_ = op;
    }

  private:
    std::shared_ptr<Device> device_;             // Target device
    uint8_t eid_;                                // Endpoint ID
    uint32_t transferHandle_;                    // Current transfer handle
    std::function<void(std::span<const uint8_t>, const MultipartRcvMeta&)>
        onData_;                                 // Chunk delivery
    std::function<void()> onComplete_;           // Completion notification
    std::function<void(std::string)> onFailure_; // Failure handler
    rde_op_id transferOperation_ =
        PLDM_RDE_XFER_FIRST_PART;                // Transfer operation state
};

} // namespace pldm::rde
