#include "libpldm/base.h"
#include "base_responder.hpp"

#include <map>
#include <vector>
#include <array>
#include <stdexcept>

namespace pldm
{
namespace responder
{

enum SupportedTypes
{
    BASE = 0x00,
};

enum SupportedCmds
{
    GET_PLDM_TYPES = 0x4,
    GET_PLDM_COMMANDS = 0x5,
};

using Cmd = std::vector<uint8_t>;

static const std::map<Type, Cmd> capabilities
{
    {BASE,
        {
        GET_PLDM_TYPES,
        GET_PLDM_COMMANDS
        }
    }
};

void getPLDMTypes()
{
    std::array<uint8_t, RESPONSE_HEADER_LEN_BYTES + GET_TYPES_RESP_DATA_BYTES>
        response{};
    // DSP0240 has this a bitfield8[N], where N = 0 to 7
    std::array<uint8_t, 8> types{};

    for (const auto& type : capabilities)
    {
        auto index = type.first / 8;
        // <Type Number> = <Array Index> * 8 + <bit position>
        auto bit = type.first - (index * 8);
        types[index] |= 1 << bit;
    }

    encode_get_types_resp(0, SUCCESS, types.data(), response.data());
}

void getPLDMCommands(Type type, ver32 version)
{
    std::array<uint8_t,
        RESPONSE_HEADER_LEN_BYTES + GET_COMMANDS_RESP_DATA_BYTES> response{};
    // DSP0240 has this a bitfield8[N], where N = 0 to 31
    std::array<uint8_t, 32> types{};

    if (capabilities.find(type) == capabilities.end())
    {
        encode_get_commands_resp(0, ERROR_INVALID_PLDM_TYPE, nullptr, nullptr);
        return;
    }

    for (const auto& cmd : capabilities.at(type))
    {
        auto index = cmd / 8;
        // <Type Number> = <Array Index> * 8 + <bit position>
        auto bit = cmd - (index * 8);
        types[index] |= 1 << bit;
    }

    encode_get_commands_resp(0, SUCCESS, types.data(), response.data());
}

} // namespace responder
} // namespace pldm
