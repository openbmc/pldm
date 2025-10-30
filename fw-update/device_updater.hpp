#pragma once

#include "common/types.hpp"
#include "requester/handler.hpp"
#include "requester/request.hpp"

#include <sdbusplus/timer.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/source/event.hpp>

#include <fstream>
#include <numeric>

namespace pldm
{

namespace fw_update
{

/** @brief Type alias for component update status tracking
 *         Maps component index to its update completion status (true indicates
 *         successful completion, false indicates cancellation)
 */
using ComponentUpdateStatusMap = std::map<size_t, bool>;

class UpdateManagerBase;

/** @class UpdateProgress
 *
 *  Attempts to provide accurate reporting of firmware update progress
 *  Called at the various phases of firmware update.
 *  A single UpdateProgress object represents a component of a pldm package
 */
class UpdateProgress
{
  public:
    /** @brief an enum to define the different states of progress
     *
     */
    enum class state
    {
        Update,
        Verify,
        Apply,
    };

    /** @brief Construct an UpdateProgress object given to the total component
     * size
     *
     * @param[in] totalSize the size in bytes of a component for a given device
     * @param[in] eid the eid of the device this class tracks progress for
     */
    UpdateProgress(uint32_t totalSize, mctp_eid_t eid);
    ~UpdateProgress() = default;
    UpdateProgress& operator=(UpdateProgress&&) = default;
    UpdateProgress& operator=(const UpdateProgress&) = delete;
    UpdateProgress(UpdateProgress&&) = default;
    UpdateProgress(const UpdateProgress&) = delete;

    /** @brief Called when switching between phases of firmware update
     *
     * @param[in] newState the state that we are entering. See
     * UpdateProgress::state
     * @return true on success, false on failure
     */
    void updateState(state newState);

    /** @brief called when servicing RequestFirmwareData commands
     *
     * @param[in] amountUpdate the length of firmware transmitted, in bytes
     * @return true on success, false on failure
     */
    void reportFwUpdate(uint32_t amountUpdated);

    /** @brief get the progress as a percentage of this component
     *
     * @return the progress of this component as an int in [0-100]
     */
    uint8_t getProgress() const;

    /** @brief get the total size of the firmware component in bytes
     *
     * @return the total number of bytes for the component
     */
    uint32_t getTotalSize() const;

  private:
    /**
     * @brief progress of firmware update in percentage [0-100]
     */
    uint8_t progress;

    /**
     * @brief the eid of the device this object tracks
     */
    mctp_eid_t eid;
    /**
     * @brief the total size in bytes of this component
     */
    uint32_t totalSize;
    /**
     * @brief the total number of bytes sent
     */
    uint32_t totalUpdated;
    /**
     * @brief the current state of update for this component
     */
    state currentState;
};

/** @class DeviceUpdater
 *
 *  DeviceUpdater orchestrates the firmware update of the firmware device and
 *  updates the UpdateManager about the status once it is complete.
 */
class DeviceUpdater
{
  public:
    DeviceUpdater() = delete;
    DeviceUpdater(const DeviceUpdater&) = delete;
    DeviceUpdater(DeviceUpdater&&) = default;
    DeviceUpdater& operator=(const DeviceUpdater&) = delete;
    DeviceUpdater& operator=(DeviceUpdater&&) = delete;
    ~DeviceUpdater() = default;

    /** @brief Constructor
     *
     *  @param[in] eid - Endpoint ID of the firmware device
     *  @param[in] package - File stream for firmware update package
     *  @param[in] fwDeviceIDRecord - FirmwareDeviceIDRecord in the fw update
     *                                package that matches this firmware device
     *  @param[in] compImageInfos - Component image information for all the
     *                              components in the fw update package
     *  @param[in] compInfo - Component info for the components in this FD
     *                        derived from GetFirmwareParameters response
     *  @param[in] maxTransferSize - Maximum size in bytes of the variable
     *                               payload allowed to be requested by the FD
     *  @param[in] updateManager - To update the status of fw update of the
     *                             device
     */
    explicit DeviceUpdater(mctp_eid_t eid, std::istream& package,
                           const FirmwareDeviceIDRecord& fwDeviceIDRecord,
                           const ComponentImageInfos& compImageInfos,
                           const ComponentInfo& compInfo,
                           uint32_t maxTransferSize,
                           UpdateManagerBase* updateManager);

    /** @brief Get the progress of updating this device as percentage
     *
     * Goes through each component for this device and calculates
     *   the percentage we are through the update process
     *
     * @return The percentage as an int [0-100], 0 is no progress, 100 is done.
     */
    uint8_t getProgress() const;

    /** @brief Start the firmware update flow for the FD
     *
     *  To start the update flow RequestUpdate command is sent to the FD.
     *
     */
    void startFwUpdateFlow();

    /** @brief Handler for RequestUpdate command response
     *
     *  The response of the RequestUpdate is processed and if the response
     *  is success, send PassComponentTable request to FD.
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    void requestUpdate(mctp_eid_t eid, const pldm_msg* response,
                       size_t respMsgLen);

    /** @brief Handler for PassComponentTable command response
     *
     *  The response of the PassComponentTable is processed. If the response
     *  indicates component can be updated, continue with either a) or b).
     *
     *  a. Send PassComponentTable request for the next component if
     *     applicable
     *  b. UpdateComponent command to request updating a specific
     *     firmware component
     *
     *  If the response indicates component may be updateable, continue
     *  based on the policy in DeviceUpdateOptionFlags.
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    void passCompTable(mctp_eid_t eid, const pldm_msg* response,
                       size_t respMsgLen);

    /** @brief Handler for UpdateComponent command response
     *
     *  The response of the UpdateComponent is processed and will wait for
     *  FD to request the firmware data.
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    void updateComponent(mctp_eid_t eid, const pldm_msg* response,
                         size_t respMsgLen);

    /** @brief Handler for RequestFirmwareData request
     *
     *  @param[in] request - Request message
     *  @param[in] payload_length - Request message payload length
     *  @return Response - PLDM Response message
     */
    Response requestFwData(const pldm_msg* request, size_t payloadLength);

    /** @brief Handler for TransferComplete request
     *
     *  @param[in] request - Request message
     *  @param[in] payload_length - Request message payload length
     *  @return Response - PLDM Response message
     */
    Response transferComplete(const pldm_msg* request, size_t payloadLength);

    /** @brief Handler for VerifyComplete request
     *
     *  @param[in] request - Request message
     *  @param[in] payload_length - Request message payload length
     *  @return Response - PLDM Response message
     */
    Response verifyComplete(const pldm_msg* request, size_t payloadLength);

    /** @brief Handler for ApplyComplete request
     *
     *  @param[in] request - Request message
     *  @param[in] payload_length - Request message payload length
     *  @return Response - PLDM Response message
     */
    Response applyComplete(const pldm_msg* request, size_t payloadLength);

    /** @brief Handler for ActivateFirmware command response
     *
     *  The response of the ActivateFirmware is processed and will update the
     *  UpdateManager with the completion of the firmware update.
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    void activateFirmware(mctp_eid_t eid, const pldm_msg* response,
                          size_t respMsgLen);
    /**
     * @brief Handler for CancelUpdateComponent command response
     *
     * @param[in] eid - Remote MCTP endpoint
     * @param[in] response - PLDM Response message
     * @param[in] respMsgLen - Response message length
     */
    void cancelUpdateComponent(mctp_eid_t eid, const pldm_msg* response,
                               size_t respMsgLen);

  private:
    /** @brief Send PassComponentTable command request
     *
     *  @param[in] compOffset - component offset in compImageInfos
     */
    void sendPassCompTableRequest(size_t offset);

    /** @brief Send UpdateComponent command request
     *
     *  @param[in] compOffset - component offset in compImageInfos
     */
    void sendUpdateComponentRequest(size_t offset);

    /** @brief Send ActivateFirmware command request */
    void sendActivateFirmwareRequest();

    /**
     * @brief Send cancel update component request
     */
    void sendCancelUpdateComponentRequest();

    /**
     * @brief Create a timer to handle RequestFirmwareData timeout (UA_T2)
     */
    void createRequestFwDataTimer();

    /** @brief Endpoint ID of the firmware device */
    mctp_eid_t eid;

    /** @brief File stream for firmware update package */
    std::istream& package;

    /** @brief FirmwareDeviceIDRecord in the fw update package that matches this
     *         firmware device
     */
    const FirmwareDeviceIDRecord& fwDeviceIDRecord;

    /** @brief Component image information for all the components in the fw
     *         update package
     */
    const ComponentImageInfos& compImageInfos;

    /** @brief Component info for the components in this FD derived from
     *         GetFirmwareParameters response
     */
    const ComponentInfo& compInfo;

    /** @brief Maximum size in bytes of the variable payload to be requested by
     *         the FD via RequestFirmwareData command
     */
    uint32_t maxTransferSize;

    /** @brief To update the status of fw update of the FD */
    UpdateManagerBase* updateManager;

    /** @brief Component index is used to track the current component being
     *         updated if multiple components are applicable for the FD.
     *         It is also used to keep track of the next component in
     *         PassComponentTable
     */
    size_t componentIndex = 0;

    /** @brief To send a PLDM request after the current command handling */
    std::unique_ptr<sdeventplus::source::Defer> pldmRequest;

    /**
     * @brief Map to hold component update status. True - success, False -
     *        cancelled
     */
    ComponentUpdateStatusMap componentUpdateStatus;

    /**
     * @brief Timeout in seconds for the UA to cancel the component update if no
     * command is received from the FD during component image transfer stage
     *
     */
    static constexpr int updateTimeoutSeconds = UPDATE_TIMEOUT_SECONDS;

    /**
     * @brief Timer to handle RequestFirmwareData timeout(UA_T2)
     *
     */
    std::unique_ptr<sdbusplus::Timer> reqFwDataTimer;
    /**
     * @brief a list of UpdateProgress objects, one for each firmware component
     *         applicable to this device
     */
    std::vector<UpdateProgress> progress;
    /**
     * @brief Whether this device has gone through application. Needed because
     *        UpdateProgress handles each component but application happens at
     *        the device level
     */
    bool activationComplete;
};

} // namespace fw_update

} // namespace pldm
