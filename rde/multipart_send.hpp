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
#include <string>
#include <vector>

namespace pldm::rde
{

class Device;

/**
 * @struct MultipartSndMeta
 * @brief Metadata describing a multipart chunk.
 *
 * Contains status and transfer information associated with a received chunk.
 */
struct MultipartSndMeta
{
    uint8_t schemaClass = 0;    ///< Schema class used for decoding.
    uint8_t hasChecksum = true; ///< Indicates presence of checksum in chunk.
    bool isFinalChunk =
        false;               ///< True if this is the final chunk in the stream.
    uint32_t nextHandle = 0; ///< Handle for the next chunk (if any).
    uint32_t checksum = 0;   ///< Optional checksum value.
    uint32_t length = 0;     ///< Length of the chunk payload.
};

/**
 * @class MultipartSender
 * @brief Orchestrates PLDM RDE multipart send operations using DSP0218.
 *
 * Supports both metadata-only requests and chunked payload transmission,
 * emitting decoded response data via callback interfaces.
 */
class MultipartSender
{
  public:
    /**
     * @brief Construct a multipart sender instance.
     * @param device Reference to target device.
     * @param eid Device endpoint ID.
     * @param transferHandle Initial transfer handle to begin sequence.
     */
    MultipartSender(std::shared_ptr<Device> device, uint8_t eid,
                    uint32_t transferHandle);

    /**
     * @brief Start a multipart read or receive flow.
     * @param onData Callback for each received chunk.
     * @param onComplete Optional callback for successful completion.
     * @param onFailure Optional callback for error handling.
     */
    void start(
        std::function<void(std::span<const uint8_t>, const MultipartSndMeta&)>
            onData,
        std::function<void()> onComplete = nullptr,
        std::function<void(std::string)> onFailure = nullptr);

    /**
     * @brief Send a command-only multipart request.
     *
     * Typically used to request next chunk from the device.
     *
     * @param handle Chunk handle to query.
     */
    void sendRequest(uint32_t handle);

    /**
     * @brief Send a data-bearing multipart request.
     *
     * Slices and transmits a payload chunk using metadata constraints.
     *
     * @param handle Handle used for this chunk's transmission.
     */
    void sendReceiveRequest(uint32_t handle);

    /**
     * @brief Handle a PLDM RDE multipart response message.
     * @param respMsg Pointer to the raw PLDM response.
     * @param rxLen Length of the response buffer.
     */
    void handleReceiveResp(const pldm_msg* respMsg, size_t rxLen);

    /**
     * @brief Set transfer operation mode.
     *
     * Controls PLDM semantics used for next multipart request.
     *
     * @param op Operation ID such as PLDM_RDE_XFER_FIRST_PART or NEXT_PART.
     */
    void setTransferOperation(rde_op_id op);

  private:
    /**
     * @brief Internal helper to dispatch a multipart command.
     *
     * Used by both `sendRequest()` and `sendReceiveRequest()` to encode and
     * register request.
     *
     * @param handle Transfer handle.
     * @param operation RDE operation code.
     * @param transferFlag Chunk flag (RDE_START, MIDDLE, END).
     * @param payload Optional payload chunk (may be empty).
     * @param checksum Optional checksum value.
     * @return True on success, false on error or registration failure.
     */
    bool sendMultipartCommand(uint32_t handle, rde_op_id operation,
                              uint8_t transferFlag,
                              const std::vector<uint8_t>& payload,
                              uint32_t checksum);

    std::shared_ptr<Device> device_;   // Target device abstraction.
    uint8_t eid_;                      // Device endpoint ID.
    uint32_t transferHandle_;          // Current multipart transfer handle.
    rde_op_id transferOperation_;      // Current transfer operation phase.

    std::vector<uint8_t> dataPayload_; // Outbound payload data.
    uint8_t transferFlag_ = PLDM_RDE_START; // Transfer flag for current chunk.

    std::function<void(std::span<const uint8_t>, const MultipartSndMeta&)>
        onData_;                                 // Chunk callback.
    std::function<void()> onComplete_;           // Completion callback.
    std::function<void(std::string)> onFailure_; // Error callback.
};

} // namespace pldm::rde
