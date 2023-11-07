#pragma once

#include "common/utils.hpp"
#include "pldmd/handler.hpp"
#include "requester/handler.hpp"

#include <libpldm/file_io.h>

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace responder
{
namespace oem_meta
{

int setupTidToSlotMappingTable();

class Handler : public CmdHandler
{
  public:
    Handler(const pldm::utils::DBusHandler* dBusIntf) : dBusIntf(dBusIntf)
    {
        handlers.emplace(PLDM_WRITE_FILE,
                         [this](const pldm_msg* request, size_t payloadLength) {
            return this->writeFileIO(request, payloadLength);
        });

        if (setupTidToSlotMappingTable() != PLDM_SUCCESS)
        {
            error("Fail to setup tid to slot mapping table");
        }
    }

    /** @brief Handler for writeFileIO command
     *
     *  @param[in] request - pointer to PLDM request payload
     *  @param[in] payloadLength - length of the message
     *
     *  @return PLDM response message
     */
    Response writeFileIO(const pldm_msg* request, size_t payloadLength);

  private:
    int handlePowerStatusChanged(char slot, uint8_t ACPIPowerStatus);

    /** @brief D-Bus Interface object*/
    const pldm::utils::DBusHandler* dBusIntf;
};

} // namespace oem_meta
} // namespace responder
} // namespace pldm
