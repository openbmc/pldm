#pragma once

#include "libbej/bej_tree.h"

#include "device_common.hpp"
#include "multipart_recv.hpp"
#include "multipart_send.hpp"

#include <libpldm/base.h>
#include <libpldm/platform.h>
#include <libpldm/rde.h>

#include <common/types.hpp>
#include <phosphor-logging/log.hpp>
#include <requester/handler.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/source/event.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pldm::rde
{

class Device; // forward declare
class MultipartReceiver;

/**
 * @class OperationSession
 * @brief Manages the RDE operation sequence for a PLDM device
 */
class OperationSession
{
  public:
    /**
     * @brief Initializes an OperationSession for executing a Redfish exchange.
     *
     * Sets up internal state and context required to manage a full Redfish
     * operation with an RDE device. Uses device-specific information and
     * operation metadata to configure session tracking, protocol state, and
     * message flow control.
     *
     * @param[in] device Reference to the target device involved in the
     * operation.
     * @param[in] oipInfo Metadata struct containing operation type, URI, and
     * format.
     */
    OperationSession(std::shared_ptr<Device> device,
                     struct OperationInfo oipInfo);

    OperationSession() = delete;
    OperationSession(const OperationSession&) = default;
    OperationSession& operator=(const OperationSession&) = delete;
    OperationSession(OperationSession&&) = default;
    OperationSession& operator=(OperationSession&&) = delete;
    ~OperationSession() = default;

    /**
     * @brief Initialize the discovery session and prepare command sequence.
     */
    void initialize();

    /**
     * @brief Attempts to update the session's operation state.
     * @param[in] newState The desired OpState
     */
    void updateState(OpState newState);

    /**
     * @brief Returns the current operation state.
     */
    OpState getState() const;

    /**
     * @brief Initiates Redfish operation with an RDE-capable device.
     *
     * This function prepares and sends the RDEOperationInit command to start a
     * Redfish operation, establishing the operation context and optionally
     * carrying inline request data. It sets flags indicating whether additional
     * payload or parameters will follow, and may trigger immediate execution
     * based on the operation setup.
     */
    void doOperationInit();

    /**
     * @brief Completes the Redfish operation with the RDE device.
     *
     * Sends the OperationComplete command to signal the end of a Redfish
     * operation, allowing the RDE device to release any resources associated
     * with the session. This marks the final step in the operation lifecycle
     * after all payload transfers are done.
     */
    void doOperationComplete();

    /**
     * @brief Handler for Operation Init Response.
     * @param eid The endpoint ID.
     * @param respMsg Pointer to the PLDM response message.
     * @param rxLen Length of the received message.
     * @param device parent device pointer.
     */
    void handleOperationInitResp(const pldm_msg* respMsg, size_t rxLen);

    /**
     * @brief Handler for Operation Complete Response.
     * @param eid The endpoint ID.
     * @param respMsg Pointer to the PLDM response message.
     * @param rxLen Length of the received message.
     * @param device parent device pointer.
     */
    void handleOperationCompleteResp(const pldm_msg* respMsg, size_t rxLen);

    inline BejDictionaries getDictionaries();

    /**
     * @brief Get the root node name in JSON payload.
     * @param json The JSON payload.
     * @return the root node string.
     */
    std::string getRootObjectName(const nlohmann::json& json);

    /**
     * @brief Create BEJ Tree for encoding.
     * @param name The root node name.
     * @param jsonObj The JSON payloan.
     * @return Pointer to the BEJ Tree.
     */
    RedfishPropertyParent* createBejTree(const std::string& name,
                                         const nlohmann::json& jsonObj);

    /**
     * @brief Get the JSON payload for BEJ data.
     * @return the JSON string payload.
     */
    std::string getJsonStrPayload();

    /**
     * @brief Get the BEJ payload for JSON data.
     * @return the BEJ payload.
     */
    std::vector<uint8_t> getBejPayload();

    /**
     * @brief Mark the operation as complete.
     */
    void markComplete();

    /**
     * @brief Check if the operation is complete.
     * @return True if complete, false otherwise.
     */
    bool isComplete() const;

    /**
     * @brief Add a chunk of response bytes to the internal buffer.
     * @param[in] payload The payload chunk to append.
     * @param[in] hasChecksum Indicates whether the payload includes a checksum.
     * @return True if the chunk was added successfully.
     */
    bool addToOperationBytes(std::span<const uint8_t> payload,
                             bool hasChecksum);

    /**
     * @brief Add a chunk of data for response buffer.
     * @param[in] resourceId The resource ID.
     * @param[in] payload The response data.
     * @param[in] hasChecksum Whether the payload includes a checksum byte.
     * @param[in] isFinalChunk Whether this is the final chunk of the
     * dictionary.
     * @throws std::invalid_argument if the payload is invalid.
     * @throws std::runtime_error if chunk processing or persistence fails.
     */
    void addChunk(uint32_t resourceId, std::span<const uint8_t> payload,
                  bool hasChecksum, bool isFinalChunk);

  private:
    std::shared_ptr<Device> device_;
    pldm::eid eid_;
    uint32_t currentResourceId_ = 0;
    OpState currentState_ = OpState::Idle;
    struct OperationInfo oipInfo;
    rde_op_id operationID = 0;
    std::vector<uint8_t> payloadBuffer;
    std::vector<uint8_t> responseBuffer;
    nlohmann::json jsonPayload;
    bool complete = false;
    bool multiPartTransferFlag = false;
    std::unique_ptr<pldm::rde::MultipartReceiver> receiver_;
    std::unique_ptr<pldm::rde::MultipartSender> sender_;
};

} // namespace pldm::rde
