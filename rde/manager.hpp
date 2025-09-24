#pragma once

#include "operation_task.hpp"
#include "requester/handler.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "xyz/openbmc_project/RDE/Manager/server.hpp"

#include <libpldm/base.h>

#include <common/instance_id.hpp>
#include <common/types.hpp>
#include <requester/handler.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>

#include <future>
#include <memory>
#include <string>

namespace pldm::rde
{

inline constexpr std::string_view RDEManagerObjectPath{
    "/xyz/openbmc_project/RDE/Manager"};
constexpr const char* DeviceObjectPath = "/xyz/openbmc_project/RDE/Device";
inline constexpr std::string_view DeviceServiceName{"xyz.openbmc_project.RDE"};

using ObjectPath = sdbusplus::message::object_path;
using OperationType =
    sdbusplus::common::xyz::openbmc_project::rde::Common::OperationType;
using PdrPayloadList = std::vector<std::vector<uint8_t>>;

class Device; // Forward declaration

/**
 * @struct DeviceContext
 * @brief Represents a Redfish Device Enablement (RDE)-capable device.
 *
 * This structure encapsulates all metadata and identifiers required to
 * manage a device participating in PLDM for RDE over MCTP. It bridges
 * internal PLDM stack representations and external D-Bus interfaces.
 *
 * ### Assumptions and Mapping:
 * - `uuid`: Globally unique identifier used during internal registration.
 * - `eid`: MCTP Endpoint ID, used internally in the PLDM stack.
 * - `tid`: Terminus ID used in PLDM stack, represented as a byte.
 * - `friendlyName`: Human-readable name for UI or logs.
 * - `devicePtr`: Pointer to the actual device object.
 */
struct DeviceContext
{
    UUID uuid;                // Unique device UUID (internal registration)
    eid deviceEID;            // MCTP Endpoint ID (internal PLDM stack)
    pldm_tid_t tid;           // Terminus ID used in PLDM stack
    std::string friendlyName; // Human-readable name for the device
    std::shared_ptr<Device> devicePtr; // Pointer to associated device object
};

/**
 * @class Manager
 * @brief Core RDE Manager class implementing the D-Bus interface for Redfish
 * Device Enablement.
 *
 * The Manager class is responsible for centralized management of the
 * system. It exposes a D-Bus interface and coordinates various aspects of
 * RDE-capable device handling and communication.
 *
 * Includes:
 *   - Exposing the RDE control interface via D-Bus.
 *   - Tracking discovered RDE-capable devices and their metadata (e.g., UUID,
 *     EID).
 *   - Managing the lifecycle of registered devices, including dynamic
 *     add/remove operations.
 *   - Forwarding Redfish-originated requests to appropriate downstream RDE
 *     targets.
 *   - Providing schema and resource discovery services to host tools or Redfish
 *     clients.
 *
 * This class is implemented as a singleton service bound to a well-known name
 * on the system bus.
 */
class Manager :
    public sdbusplus::server::object::object<
        sdbusplus::xyz::openbmc_project::RDE::server::Manager>,
    public pldm::MctpDiscoveryHandlerIntf
{
  public:
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) = delete;
    Manager& operator=(Manager&&) = delete;

    ~Manager() = default;

    /**
     * @brief Construct an RDE Manager object.
     *
     * Initializes the RDE Manager interface on the specified D-Bus path,
     * enabling support for Redfish Device Enablement operations.
     *
     * @param[in] bus           Reference to the system D-Bus connection.
     * @param[in] event         pldmd sd_event loop
     * @param[in] instanceIdDb  Pointer to the instance ID database used for
     *                          PLDM message tracking.
     * @param[in] handler       Pointer to the PLDM request handler for sending
     *                          and receiving messages.
     */
    Manager(sdbusplus::bus::bus& bus, sdeventplus::Event& event,
            pldm::InstanceIdDb* instanceIdDb,
            pldm::requester::Handler<pldm::requester::Request>* handler);

    /**
     * @brief Get the instance ID database pointer.
     *
     * @return Pointer to the instance ID database.
     */
    inline pldm::InstanceIdDb* getInstanceIdDb() const
    {
        return instanceIdDb_;
    }

    /**
     * @brief Get the PLDM request handler pointer.
     *
     * @return Pointer to the PLDM request handler.
     */
    inline pldm::requester::Handler<pldm::requester::Request>* getHandler()
        const
    {
        return handler_;
    }

    /**
     * @brief Get the D-Bus connection reference.
     *
     * @return Reference to the D-Bus connection.
     */
    inline sdbusplus::bus::bus& getBus()
    {
        return bus_;
    }

    /**
     * @brief Retrieves a pointer to the DeviceContext by eid.
     * @param deviceEID[in] MCTP Endpoint ID
     * @return Pointer to DeviceContext or nullptr if not found
     */
    DeviceContext* getDeviceContext(eid deviceEID);

    /**
     * @brief Implementation for StartRedfishOperation
     * Begin execution of a Redfish operation on the target device. Returns a
     * D-Bus object path for tracking asynchronous progress through an
     * OperationTask instance.
     *
     * @param[in] operationID - Unique identifier for this operation lifecycle.
     *            This ID must be supplied by the caller and reused for any
     *            related commands (e.g., cancellation, polling). Enables
     *            multi-phase tracking and client-side logical correlation.
     * @param[in] operationType - Type of Redfish operation to perform.
     * @param[in] targetURI - URI of the target Redfish resource or action.
     * @param[in] deviceUUID - Uniquely identifies the target device instance.
     * @param[in] eid - Optional endpoint identifier for MCTP or similar
     *            transport. Omit for protocols that do not use EID or when
     *            resolved internally.
     * @param[in] payload - Redfish payload content, either embedded inline as
     *            a JSON/BEJ string, or provided as an external file path
     *            reference.
     * @param[in] payloadFormat - Specifies whether the payload is embedded
     *            inline or referenced via external file path.
     * @param[in] encodingFormat - Encoding format of the payload. Choose
     * 'JSON' for plain text or 'BEJ' for compact binary. JSON is the default
     * for broader compatibility.
     * @param[in] sessionId - Optional grouping label used to associate
     * multiple operations under a logical client session. Useful for
     * long-running tasks, audit logging, or correlation across coordinated
     * workflows.
     *
     * @return path[sdbusplus::message::object_path] - D-Bus object path for
     *         the xyz.openbmc_project.RDE.OperationTask created for this
     *         operation. This object allows clients to monitor execution
     *         progress, retrieve response metadata, and manage task lifecycle.
     */
    ObjectPath startRedfishOperation(
        uint32_t operationID,
        sdbusplus::common::xyz::openbmc_project::rde::Common::OperationType
            operationType,
        std::string targetURI, std::string deviceUUID, uint8_t eid,
        std::string payload, PayloadFormatType payloadFormat,
        EncodingFormatType encodingFormat, std::string sessionId) override;

    /** @brief Implementation for GetSupportedOperations
     *  Report the list of supported Redfish operations for a specified target
     * device.
     *
     *  @param[in] deviceUUID - Uniquely identifies the target device.
     *
     *  @return
     * supportedOps[std::vector<sdbusplus::common::xyz::openbmc_project::rde::Common::OperationType>]
     * - Operations supported by the device, determined via capability
     * discovery.
     */
    std::vector<OperationType> getSupportedOperations(
        std::string deviceUUID) override;

    /** @brief Implementation for GetDeviceSchemaInfo
     *  Retrieve the schema metadata for a specific target device as exposed via
     * SchemaResources. This includes Redfish-compatible URI fragments, schema
     * types, and supported operations.
     *
     *  @param[in] deviceUUID - Uniquely identifies the target device.
     *
     *  @return schemaInfo[std::map<std::string, std::map<std::string,
     * std::variant<int64_t, std::string>>>] - Dictionary of schema metadata
     * indexed by resource ID, aligned with the SchemaResourceKeys definition.
     */
    std::map<std::string,
             std::map<std::string, std::variant<int64_t, std::string>>>
        getDeviceSchemaInfo(std::string deviceUUID) override;

    /** @brief Helper function to invoke registered handlers for
     *         the added MCTP endpoints
     *
     *  @param[in] mctpInfos - information of discovered MCTP endpoints
     */
    void handleMctpEndpoints(const MctpInfos& mctpInfos) override;

    /** @brief Helper function to invoke registered handlers for
     *         the removed MCTP endpoints
     *
     *  @param[in] mctpInfos - information of removed MCTP endpoints
     */
    void handleRemovedMctpEndpoints(const MctpInfos&) override
    {
        return;
    }

    /** @brief Helper function to invoke registered handlers for
     *  updating the availability status of the MCTP endpoint
     *
     *  @param[in] mctpInfo - information of the target endpoint
     *  @param[in] availability - new availability status
     */
    void updateMctpEndpointAvailability(const MctpInfo&, Availability) override
    {
        return;
    }

    /** @brief Get Active EIDs.
     *
     *  @param[in] addr - MCTP address of terminus
     *  @param[in] terminiNames - MCTP terminus name
     */
    std::optional<mctp_eid_t> getActiveEidByName(const std::string&) override
    {
        return std::nullopt;
    }

    /**
     * @brief Create a device D-Bus object associated with PLDM discovery.
     *
     * This method initializes and registers a D-Bus object representing a
     * discovered device, using its endpoint ID (EID), unique identifier (UUID),
     * and type identifier (TID). It is typically invoked during the PLDM
     * discovery session as part of Redfish Device Enablement.
     *
     * @param deviceEID[in]   Endpoint ID of the device as reported during MCTP
     * discovery.
     * @param devUUID[in]  Unique identifier string for the device.
     * @param tid[in]   Type ID of the device (PLDM Terminus ID).
     * @param pdrPayloads[in] Vector of raw Redfish Resource PDR payloads,
     *        each PDR as a vector of bytes (std::vector<uint8_t>)
     */
    void createDeviceDbusObject(eid deviceEID, const UUID& devUUID,
                                pldm_tid_t tid,
                                const PdrPayloadList& pdrPayloads);

    /**
     * @brief Retrieve the map of currently active OperationTask D-Bus objects.
     *
     * This method provides direct access to the internal task registry,
     * where each Operation ID is mapped to its corresponding D-Bus object.
     * These objects expose the Redfish operation status via the
     * xyz.openbmc_project.RDE.OperationTask interface.
     *
     * @return Reference to the task map: operationID → OperationTask smart
     * pointer.
     */
    const std::unordered_map<uint32_t, std::shared_ptr<OperationTaskIface>>&
        getActiveTasks() const
    {
        return taskMap_;
    }

  private:
    pldm::InstanceIdDb* instanceIdDb_ = nullptr;
    pldm::requester::Handler<pldm::requester::Request>* handler_ = nullptr;
    sdbusplus::bus::bus& bus_;
    sdeventplus::Event& event_;
    std::unordered_map<eid, DeviceContext> eidMap_;
    std::unique_ptr<sdbusplus::bus::match_t> signalMatch_;
    std::unordered_map<eid, std::unique_ptr<sdbusplus::bus::match_t>>
        signalMatches_;
    // Registry of active OperationTask D-Bus objects.
    std::unordered_map<uint32_t, // OperationID
                       std::shared_ptr<OperationTaskIface>>
        taskMap_;
    std::unique_ptr<sdbusplus::server::manager_t> objManager_;
};

} // namespace pldm::rde
