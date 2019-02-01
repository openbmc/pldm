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

void getPLDMTypes(const pldm_msg_payload* request, pldm_msg* response)
{
    // DSP0240 has this as a bitfield8[N], where N = 0 to 7
    std::array<uint8_t, 8> types{};
    for (const auto& type : capabilities)
    {
        auto index = type.first / 8;
        // <Type Number> = <Array Index> * 8 + <bit position>
        auto bit = type.first - (index * 8);
        types[index] |= 1 << bit;
    }

    encode_get_types_resp(0, PLDM_SUCCESS, types.data(), response);
}

void getPLDMCommands(const pldm_msg_payload* request, pldm_msg* response)
{
    pldm_version version{};
    Type type;

    if (request->payload_length != (sizeof(version) + sizeof(type)))
    {
        encode_get_commands_resp(0, PLDM_ERROR_INVALID_LENGTH, nullptr,
                                 response);
        return;
    }

    decode_get_commands_req(request, &type, &version);

    // DSP0240 has this as a bitfield8[N], where N = 0 to 31
    std::array<uint8_t, 32> cmds{};
    if (capabilities.find(type) == capabilities.end())
    {
        encode_get_commands_resp(0, PLDM_ERROR_INVALID_PLDM_TYPE, nullptr,
                                 response);
        return;
    }

    for (const auto& cmd : capabilities.at(type))
    {
        auto index = cmd / 8;
        // <Type Number> = <Array Index> * 8 + <bit position>
        auto bit = cmd - (index * 8);
        cmds[index] |= 1 << bit;
    }

    encode_get_commands_resp(0, PLDM_SUCCESS, cmds.data(), response);
}

} // namespace responder
} // namespace pldm
