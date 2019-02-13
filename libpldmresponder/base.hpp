#pragma once

#include <stdint.h>

#include <vector>

namespace pldm
{

using Type = uint8_t;

namespace responder
{

using Response = std::vector<uint8_t>;
using Request = const uint8_t*;

Response getPLDMTypes(Request payload, size_t payloadLen);
Response getPLDMCommands(Request payload, size_t payloadLen);

void GetPLDMVersion(uint32_t transferHandle, uint8_t transferFlag, Type type);

} // namespace responder
} // namespace pldm
