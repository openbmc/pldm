#include "libpldm/base.h"

#include "base.hpp"

#include <array>
#include <cstring>
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

static const std::map<Type, ver32_t> versions{
    {PLDM_BASE, {0xF1, 0xF0, 0xF0, 0x00}},
};

void getPLDMTypes(const pldm_msg* request, pldm_msg* response)
{
    // DSP0240 has this as a bitfield8[N], where N = 0 to 7
    std::array<bitfield8_t, 8> types{};
    for (const auto& type : capabilities)
    {
        auto index = type.first / 8;
        // <Type Number> = <Array Index> * 8 + <bit position>
        auto bit = type.first - (index * 8);
        types[index].byte |= 1 << bit;
    }

    encode_get_types_resp(0, PLDM_SUCCESS, types.data(), response);
}

void getPLDMCommands(const pldm_msg* request, pldm_msg* response,
                     uint8_t payload_length)
{
    ver32_t version{};
    Type type;

    if (payload_length != (sizeof(version) + sizeof(type)))
    {
        encode_get_commands_resp(0, PLDM_ERROR_INVALID_DATA, nullptr, response);
        return;
    }

    decode_get_commands_req(request->payload, &type, &version);

    // DSP0240 has this as a bitfield8[N], where N = 0 to 31
    std::array<bitfield8_t, 32> cmds{};
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
        cmds[index].byte |= 1 << bit;
    }

    encode_get_commands_resp(0, PLDM_SUCCESS, cmds.data(), response);
}

void getPLDMVersion(const pldm_msg* request, pldm_msg* response,
                    uint8_t payload_length)
{
    uint32_t transferHandle;
    Type type;
    uint8_t transferFlag;

    if (payload_length !=
        (sizeof(transferHandle) + sizeof(type) + sizeof(transferFlag)))
    {
        encode_get_version_resp(0, PLDM_ERROR_INVALID_LENGTH, 0, 0, nullptr, 0,
                                response);
        return;
    }

    decode_get_version_req(request->payload, &transferHandle, &transferFlag,
                           &type);

    ver32_t version{};
    auto search = versions.find(type);

    if (search == versions.end())
    {
        encode_get_version_resp(0, PLDM_ERROR_INVALID_PLDM_TYPE, 0, 0, nullptr,
                                0, response);
        return;
    }

    memcpy(&version, &(search->second), sizeof(version));
    encode_get_version_resp(0, PLDM_SUCCESS, 0, PLDM_START_AND_END, &version,
                            sizeof(pldm_version), response);
}

} // namespace responder
} // namespace pldm
