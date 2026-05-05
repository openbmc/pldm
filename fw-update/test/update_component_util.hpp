#pragma once

#include "fw-update/device_updater.hpp"

#include <libpldm/firmware_update.h>

#include <cstdint>
#include <vector>

namespace pldm::fw_update::test
{

/** @brief A successful UpdateComponent response (component can be updated)
 *
 *  completion_code, comp_compatibility_resp, comp_compatibility_resp_code,
 *  update_option_flags_enabled and time_before_req_fw_data are all zero.
 */
inline std::vector<uint8_t> makeUpdateComponentOkResponse(uint8_t instanceId)
{
    std::vector<uint8_t> msg(
        sizeof(pldm_msg_hdr) + sizeof(pldm_update_component_resp), 0);
    auto* hdr = reinterpret_cast<pldm_msg*>(msg.data());
    hdr->hdr.request = PLDM_RESPONSE;
    hdr->hdr.type = PLDM_FWUP;
    hdr->hdr.command = PLDM_UPDATE_COMPONENT;
    hdr->hdr.instance_id = instanceId;
    return msg;
}

/** @brief Move a DeviceUpdater into the DOWNLOAD phase
 *
 *  The FD may send RequestFirmwareData only after a successful
 *  UpdateComponent response (DSP0267 Sec. 12.6), so tests that exercise
 *  requestFwData() feed the updater that response first.
 */
inline void advanceToDownload(DeviceUpdater& updater, mctp_eid_t eid = 0)
{
    auto resp = makeUpdateComponentOkResponse(0x42);
    auto* respMsg = reinterpret_cast<const pldm_msg*>(resp.data());
    updater.updateComponent(eid, respMsg, resp.size() - sizeof(pldm_msg_hdr));
}

} // namespace pldm::fw_update::test
