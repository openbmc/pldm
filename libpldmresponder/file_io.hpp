#pragma once

#include <stdint.h>

#include <vector>

#include "libpldm/base.h"
#include "libpldm/file_io.h"

namespace pldm
{

using Type = uint8_t;

namespace responder
{

namespace fileio
{

void registerHandlers();

}

/** @brief Handler for ReadFileInto
 *
 *  @param[in] request - Request message payload
 *  @param[out] response - Response message written here
 */
void readFileIntoMemory(const pldm_msg_payload* request, pldm_msg* response);

} // namespace responder
} // namespace pldm
