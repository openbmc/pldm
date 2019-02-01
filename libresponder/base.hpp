#pragma once

namespace pldm
{

using Type = uint8_t;

namespace responder
{

void getPLDMTypes();

void getPLDMCommands(Type type, ver32 version);

} // namespace responder
} // namespace pldm
