#pragma once

#include "common/types.hpp"
#include "device_common.hpp"
#include "multipart_recv.hpp"
#include "requester/handler.hpp"

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
 * @class DiscoverySession
 * @brief Manages the RDE discovery sequence for a PLDM device
 */
class DiscoverySession
{
  public:
    /**
     * @brief Constructs a DiscoverySession worker using device metadata.
     *
     * This constructor initializes the discovery session by extracting required
     * components—such as the PLDM Instance ID database, request handler, and
     * endpoint identifier—from the provided device context. These parameters
     * are essential for performing protocol-specific discovery operations on
     * the detected device.
     *
     * @param[in] device A reference to the Device object containing discovery
     * dependencies.
     */
    explicit DiscoverySession(std::shared_ptr<Device> device);

    DiscoverySession() = delete;
    DiscoverySession(const DiscoverySession&) = delete;
    DiscoverySession& operator=(const DiscoverySession&) = delete;
    DiscoverySession(DiscoverySession&&) = default;
    DiscoverySession& operator=(DiscoverySession&&) = delete;
    ~DiscoverySession() = default;

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
     * @brief Sends a RDE asynchronously.
     *
     * This coroutine prepares and dispatches a PLDM request message to the
     * target device, and registers a response handler. It uses an instance ID
     * to track the request lifecycle.
     *
     * @param eid Target endpoint ID of the device.
     * @param instanceId Instance ID used to tag the outgoing PLDM request.
     * @param command RDE command code to be executed.
     * @param request Constructed PLDM request payload.
     * @param callback Callback to invoke upon receiving the response message.
     *
     * @throws std::runtime_error If the request fails to send.
     */

    void sendRDECommand(uint8_t eid, uint8_t instanceId, uint8_t command,
                        Request&& request,
                        std::function<void(const pldm_msg*, size_t)> callback);

    /**
     * @brief Executes the Redfish negotiation sequence.
     *
     * Initiates the RDE NegotiateRedfishParameters command to establish
     * communication parameters between the management controller and
     * the Redfish-capable device.
     *
     */
    void doNegotiateRedfish();

    /**
     * @brief Handler for Negotiate Redfish Response.
     * @param[in] respMsg Pointer to the PLDM response message.
     * @param[in] rxLen Length of the received message.
     */
    void handleNegotiateRedfishResp(const pldm_msg* respMsg, size_t rxLen);

    /**
     * @brief Executes the Medium Parameter negotiation sequence.
     *
     * Initiates the RDE NegotiateMediumParameters command to establish
     * communication parameters between the management controller and
     * the Redfish-capable device.
     */
    void doNegotiateMediumParams();

    /**
     * @brief Handler for Negotiate Redfish Response.
     * @param respMsg Pointer to the PLDM response message.
     * @param rxLen Length of the received message.
     */
    void handleNegotiateMediumResp(const pldm_msg* respMsg, size_t rxLen);

    /**
     * @brief Initiates the dictionary retrieval phase for major schema
     * resources.
     *
     * This function queries the ResourceRegistry associated with the current
     * device to collect all available resource IDs with a schema type of
     * PLDM_RDE_SCHEMA_MAJOR. The gathered resources are stored for sequential
     * command dispatch using the RDE GetSchemaDictionary request.
     *
     * Once the resource list is populated, an sdeventplus::Defer source is
     * created to begin asynchronous execution of dictionary commands via
     * runNextDictionaryCommand(). Dictionary responses are handled with support
     * for multipart chunk reception.
     *
     * @note This phase is triggered after the Medium Parameter command
     * completes.
     */
    void getDictionaries();

    /**
     * @brief Executes the next command in the dictionary command queue.
     *
     * This function processes the command located at the specified index in the
     * dictionary command list. It is typically used to trigger command
     * execution in a predefined sequence.
     *
     * @param index The position in the command list to execute. Must be a valid
     *        index within the bounds of the command list.
     */
    void runNextDictionaryCommand(size_t index);

    /**
     * @brief Handles the PLDM GetSchemaDictionary response from the device.
     *
     * Parses and processes the response to the RDE GetSchemaDictionary command,
     * which includes metadata such as the dictionary format and TransferHandle
     *
     * @param[in] respMsg    Pointer to the received PLDM response message.
     * @param[in] rxLen      Length of the received message payload in bytes.
     */
    void handleGetSchemaDictionaryResp(const pldm_msg* respMsg, size_t rxLen);

  private:
    std::shared_ptr<Device> device_;
    pldm::eid eid_;
    pldm_tid_t tid_;
    bool initialized_ = false;
    OpState currentState_ = OpState::Idle;
    std::vector<std::pair<uint32_t, pldm_rde_schema_type>>
        majorSchemaResources_;
    size_t resourceIndex_ = 0;
    std::unique_ptr<sdeventplus::source::Defer> dictionaryDefer_;
    // These hold the context across event-driven steps
    uint32_t currentResourceId_ = 0;
    pldm_rde_schema_type currentSchemaClass_ =
        static_cast<pldm_rde_schema_type>(0);
    std::unique_ptr<pldm::rde::MultipartReceiver> receiver_;
};

} // namespace pldm::rde
