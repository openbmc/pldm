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

using Cmd = std::vector<uint8_t>;

static const std::map<Type, Cmd> capabilities{
    {PLDM_BASE, {PLDM_GET_PLDM_TYPES, PLDM_GET_PLDM_COMMANDS}}};

void getPLDMTypes(const pldm_msg_t* request, size_t payloadLen,
                  pldm_msg_t* response)
{
    std::array<uint8_t, PLDM_GET_TYPES_RESP_DATA_BYTES> responsePayload{};
    response->payload = responsePayload.data();
    // DSP0240 has this a bitfield8[N], where N = 0 to 7
    std::array<uint8_t, 8> types{};

    for (const auto& type : capabilities)
    {
        auto index = type.first / 8;
        // <Type Number> = <Array Index> * 8 + <bit position>
        auto bit = type.first - (index * 8);
        types[index] |= 1 << bit;
    }

    encode_get_types_resp(0, types.data(), response);
}

void getPLDMCommands(const pldm_msg_t* request, size_t payloadLen,
                     pldm_msg_t* response)
{
    pldm_version_t version{};
    Type type;
    std::array<uint8_t, PLDM_GET_COMMANDS_RESP_DATA_BYTES> responsePayload{};
    response->payload = responsePayload.data();

    if (payloadLen != (sizeof(version) + sizeof(type)))
    {
        response->payload[0] = PLDM_ERROR_INVALID_LENGTH;
        encode_get_commands_resp(0, nullptr, response);
        return;
    }

    decode_get_commands_req(request, &type, &version);

    // DSP0240 has this a bitfield8[N], where N = 0 to 31
    std::array<uint8_t, 32> cmds{};

    if (capabilities.find(type) == capabilities.end())
    {
        response->payload[0] = PLDM_ERROR_INVALID_PLDM_TYPE;
        encode_get_commands_resp(0, nullptr, response);
        return;
    }

    for (const auto& cmd : capabilities.at(type))
    {
        auto index = cmd / 8;
        // <Type Number> = <Array Index> * 8 + <bit position>
        auto bit = cmd - (index * 8);
        cmds[index] |= 1 << bit;
    }

    encode_get_commands_resp(0, cmds.data(), response);
}

} // namespace responder
} // namespace pldm
