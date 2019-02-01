#include "libpldm/base.h"

#include "base.hpp"

#include <array>
#include <map>
#include <stdexcept>
#include <vector>

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

static const std::map<Type, Cmd> capabilities{
    {BASE, {GET_PLDM_TYPES, GET_PLDM_COMMANDS}}};

Response getPLDMTypes(Request payload, size_t payloadLen)
{
    std::vector<uint8_t> response{};
    response.resize(RESPONSE_HEADER_LEN_BYTES + GET_TYPES_RESP_DATA_BYTES);

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
    return response;
}

Response getPLDMCommands(Request payload, size_t payloadLen)
{
    ver32 version{};
    Type type;
    std::vector<uint8_t> response{};
    response.resize(RESPONSE_HEADER_LEN_BYTES + GET_COMMANDS_RESP_DATA_BYTES);

    if (payloadLen != (sizeof(version) + sizeof(type)))
    {
        encode_get_commands_resp(0, ERROR_INVALID_LENGTH, nullptr,
            response.data());
        return response;
    }

    decode_get_commands_req(payload, &type, &version);

    // DSP0240 has this a bitfield8[N], where N = 0 to 31
    std::array<uint8_t, 32> types{};

    if (capabilities.find(type) == capabilities.end())
    {
        encode_get_commands_resp(0, ERROR_INVALID_PLDM_TYPE, nullptr,
            response.data());
        return response;
    }

    for (const auto& cmd : capabilities.at(type))
    {
        auto index = cmd / 8;
        // <Type Number> = <Array Index> * 8 + <bit position>
        auto bit = cmd - (index * 8);
        types[index] |= 1 << bit;
    }

    encode_get_commands_resp(0, SUCCESS, types.data(), response.data());
    return response;
}

} // namespace responder
} // namespace pldm
