#pragma once

#include "pldmd_api.hpp"

#include <functional>
#include <map>

#include "libpldm/base.h"

using namespace pldm::daemon_api;

namespace pldm
{

struct Request
{
    uint8_t destinationId;
    const pldm_msg* msg;
};

using Response = std::vector<uint8_t>;

namespace responder
{

using Handler = std::function<Response(
    const Interfaces& intfs, const Request& request, size_t payloadLength)>;

/** @brief Register a handler for input PLDM command
 *
 *  @param[in] pldmType - PLDM type code
 *  @param[in] pldmCommand - PLDM command code
 *  @param[in] handler - PLDM command handler
 */
void registerHandler(uint8_t pldmType, uint8_t pldmCommand, Handler&& handler);

/** @brief Invoke a handler for input PLDM command
 *
 *  @param[in] pldmType - PLDM type code
 *  @param[in] pldmCommand - PLDM command code
 *  @param[in] request - PLDM request message
 *  @param[in] payloadLength - PLDM request message length
 *  @return PLDM Response message. Empty response indicates async handler.
 */
Response invokeHandler(const Interfaces& intfs, uint8_t pldmType,
                       uint8_t pldmCommand, const Request& request,
                       size_t payloadLength);

} // namespace responder
} // namespace pldm
