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

static const std::map<Type, uint32_t> version_map{
    {PLDM_BASE, 0xF3F71061} // TODO need to put exact version
};

Response getPLDMTypes(Request payload, size_t payloadLen)
{
    std::vector<uint8_t> response{};
    response.resize(PLDM_RESPONSE_HEADER_LEN_BYTES +
                    PLDM_GET_TYPES_RESP_DATA_BYTES);

    // DSP0240 has this a bitfield8[N], where N = 0 to 7
    std::array<uint8_t, 8> types{};

    for (const auto& type : capabilities)
    {
        auto index = type.first / 8;
        // <Type Number> = <Array Index> * 8 + <bit position>
        auto bit = type.first - (index * 8);
        types[index] |= 1 << bit;
    }

    encode_get_types_resp(0, PLDM_SUCCESS, types.data(), response.data());
    return response;
}

Response getPLDMCommands(Request payload, size_t payloadLen)
{
    pldm_version_t version{};
    Type type;
    std::vector<uint8_t> response{};
    response.resize(PLDM_RESPONSE_HEADER_LEN_BYTES +
                    PLDM_GET_COMMANDS_RESP_DATA_BYTES);

    if (payloadLen != (sizeof(version) + sizeof(type)))
    {
        encode_get_commands_resp(0, PLDM_ERROR_INVALID_LENGTH, nullptr,
                                 response.data());
        return response;
    }

    decode_get_commands_req(payload, &type, &version);

    // DSP0240 has this a bitfield8[N], where N = 0 to 31
    std::array<uint8_t, 32> types{};

    if (capabilities.find(type) == capabilities.end())
    {
        encode_get_commands_resp(0, PLDM_ERROR_INVALID_PLDM_TYPE, nullptr,
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

    encode_get_commands_resp(0, PLDM_SUCCESS, types.data(), response.data());
    return response;
}

void GetPLDMVersion(uint32_t transferHandle, uint8_t transferFlag, uint8_t type)
{
    // TODO multipart transfer
    std::array<uint8_t, PLDM_RESPONSE_HEADER_LEN_BYTES +
                            PLDM_GET_VERSION_RESP_DATA_BYTES>
        response{};

    auto version = new uint32_t;
    auto search = version_map.find(type);

    if (search == version_map.end())
    {
        encode_get_version_resp(0, PLDM_ERROR_INVALID_PLDM_TYPE, 0, 0, nullptr,
                                0, nullptr, 0);
        return;
    }
    *version = search->second;
    encode_get_version_resp(0, PLDM_SUCCESS, 0x0, 0x05, version,
                            sizeof(uint32_t), response.data(), response.size());
}

} // namespace responder
} // namespace pldm
