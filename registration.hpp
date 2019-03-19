#pragma once

#include <functional>
#include <map>

#include "libpldm/base.h"

namespace pldm
{

namespace responder
{

using Handler =
    std::function<void(const pldm_msg_payload* request, pldm_msg* response)>;

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
void invokeHandler(uint8_t pldmType, uint8_t pldmCommand,
                   const pldm_msg_payload* request, pldm_msg* response);

namespace base
{
/** @brief Register handlers for command from the base spec
 */
void registerHandlers();
} // namespace base
} // namespace responder
} // namespace pldm
