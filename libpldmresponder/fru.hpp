#pragma once

#include "config.h"

#include <vector>

#include "libpldm/fru.h"

namespace pldm
{

using Response = std::vector<uint8_t>;

namespace responder
{

namespace fru
{

/** @brief Register handlers for commands from the FRU spec
 */
void registerHandlers();

} // namespace fru

/** @brief Handler for GetFRURecordTableMetadata
 *
 *  @param[in] request - Request message payload
 *  @param[in] payloadLength - Request payload length
 *  @param[out] Response - Response message written here
 */
Response getFRURecordTableMetadata(const pldm_msg* request, size_t payloadLength);

} // namespace responder
} // namespace pldm
