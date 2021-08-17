#pragma once

#include "common/types.hpp"
#include "requester/handler.hpp"
#include "requester/request.hpp"

#include <sdeventplus/event.hpp>
#include <sdeventplus/source/event.hpp>

#include <fstream>

namespace pldm
{

namespace fw_update
{

using namespace sdeventplus;
using namespace sdeventplus::source;
using namespace pldm::dbus_api;

class UpdateManager;

class DeviceUpdater
{
  public:
    DeviceUpdater() = delete;
    DeviceUpdater(const DeviceUpdater&) = delete;
    DeviceUpdater(DeviceUpdater&&) = default;
    DeviceUpdater& operator=(const DeviceUpdater&) = delete;
    DeviceUpdater& operator=(DeviceUpdater&&) = default;
    ~DeviceUpdater() = default;

    const uint32_t maxTransferSize = 512;

    /** @brief Constructor
     *
     *  @param[in] eid - endpoint ID of the firmware device
     *  @param[in] event - reference to PLDM daemon's main event loop
     *  @param[in] requester - reference to Requester object
     *  @param[in] handler - PLDM request handler
     *  @param[in] package - stream object to read from the fw update package
     *  @param[in] fwDeviceIDRecord - FirmwareDeviceIDRecord from the fw update
     *                                package for this device
     *  @param[in] compImageInfos - component image information for all the
     *                              components in the fw update package
     *  @param[in] compInfo - component info derived from
     *                           GetFirmwareParameters response
     *  @param[in] updateManager - pointer to the UpdateManager to update
     *                             the status of the device fw update
     */
    explicit DeviceUpdater(
        mctp_eid_t eid, Event& event, Requester& requester,
        pldm::requester::Handler<pldm::requester::Request>& handler,
        std::ifstream& package, const FirmwareDeviceIDRecord& fwDeviceIDRecord,
        const ComponentImageInfos& compImageInfos,
        const ComponentInfo& compInfo, UpdateManager* updateManager) :
        eid(eid),
        event(event), requester(requester), handler(handler), package(package),
        fwDeviceIDRecord(fwDeviceIDRecord), compImageInfos(compImageInfos),
        compInfo(compInfo), updateManager(updateManager)
    {
        const auto& applicableCompVec =
            std::get<ApplicableComponents>(fwDeviceIDRecord);
        size_t vecEntryCount = 0;
        for (const auto& vecEntry : applicableCompVec)
        {
            for (size_t idx = 0; idx < vecEntry.size(); idx++)
            {
                if (vecEntry[idx])
                {
                    applicableCompOffsets.emplace_back(
                        idx + (vecEntryCount * vecEntry.size()));
                }
            }
            vecEntryCount++;
        }
    }

    /** @brief Start the firmware update flow for the FD
     *
     */
    void startFwUpdateFlow();

    /** @brief Handler for RequestUpdate command response
     *
     *  The response of the RequestUpdate is processed and if the response
     * is success, send PassComponentTable request to FD.
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
     * applicable b. UpdateComponent command to request updating a specific
     * firmware component
     *
     *  If the response indicates component may be updateable, continue
     * based on the policy in DeviceUpdateOptionFlags.
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    void passCompTable(mctp_eid_t eid, const pldm_msg* response,
                       size_t respMsgLen);

    /** @brief Handler for UpdateComponent command response
     *
     *  The response of the UpdateComponent is processed and send
     *  PassComponent table for the next component if applicable or send
     *  UpdateComponent command to request updating a specific firmware
     *  component.
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
     *  The response of the UpdateComponent is processed and send
     *  PassComponent table for the next component if applicable or send
     *  UpdateComponent command to request updating a specific firmware
     *  component.
     *
     *  @param[in] eid - Remote MCTP endpoint
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - Response message length
     */
    void activateFirmware(mctp_eid_t eid, const pldm_msg* response,
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

    void reportSuccess();

    void reportFailure();

    mctp_eid_t eid;       //!< file descriptor of MCTP communications socket
    Event& event;         //!< reference to PLDM daemon's main event loop
    Requester& requester; //!< reference to Requester object
    /** @brief PLDM request handler */
    pldm::requester::Handler<pldm::requester::Request>& handler;
    std::ifstream& package;
    const FirmwareDeviceIDRecord& fwDeviceIDRecord;
    const ComponentImageInfos& compImageInfos;
    const ComponentInfo& compInfo;
    UpdateManager* updateManager;
    std::vector<size_t> applicableCompOffsets;
    size_t componentIndex = 0;
    std::unique_ptr<sdeventplus::source::Defer> pldmRequest;
};

} // namespace fw_update

} // namespace pldm