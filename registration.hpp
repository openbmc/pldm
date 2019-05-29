#pragma once

#include <functional>
#include <map>

#include "libpldm/base.h"

namespace pldm
{

using Response = std::vector<uint8_t>;

namespace responder
{

using Handler =
    std::function<Response(const pldm_msg* request, size_t payloadLength)>;

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
 *  @param[in] request - PLDM request message's payload
 *  @param[out] response - PLDM response message
 */
Response invokeHandler(uint8_t pldmType, uint8_t pldmCommand,
                       const pldm_msg* request, size_t payloadLength);

} // namespace responder
} // namespace pldm
