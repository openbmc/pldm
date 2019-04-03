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

std::vector<uint8_t> getPLDMTypes(const pldm_msg* request, size_t payloadLength)
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

    std::vector<uint8_t> response(
        sizeof(pldm_msg_hdr) + PLDM_GET_TYPES_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    encode_get_types_resp(0, PLDM_SUCCESS, types.data(), responsePtr);

    return response;
}

std::vector<uint8_t> getPLDMCommands(const pldm_msg* request,
                                     size_t payloadLength)
{
    ver32_t version{};
    Type type;

    std::vector<uint8_t> response(
        sizeof(pldm_msg_hdr) + PLDM_GET_COMMANDS_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    uint8_t rc = decode_get_commands_req(request->payload, payloadLength, &type,
                                         &version);

    if (rc != PLDM_SUCCESS)
    {
        encode_get_commands_resp(0, rc, nullptr, responsePtr);
        return response;
    }

    // DSP0240 has this as a bitfield8[N], where N = 0 to 31
    std::array<bitfield8_t, 32> cmds{};
    if (capabilities.find(type) == capabilities.end())
    {
        encode_get_commands_resp(0, PLDM_ERROR_INVALID_PLDM_TYPE, nullptr,
                                 responsePtr);
        return response;
    }

    for (const auto& cmd : capabilities.at(type))
    {
        auto index = cmd / 8;
        // <Type Number> = <Array Index> * 8 + <bit position>
        auto bit = cmd - (index * 8);
        cmds[index].byte |= 1 << bit;
    }

    encode_get_commands_resp(0, PLDM_SUCCESS, cmds.data(), responsePtr);

    return response;
}

std::vector<uint8_t> getPLDMVersion(const pldm_msg* request,
                                    size_t payloadLength)
{
    uint32_t transferHandle;
    Type type;
    uint8_t transferFlag;

    std::vector<uint8_t> response(
        sizeof(pldm_msg_hdr) + PLDM_GET_VERSION_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    uint8_t rc = decode_get_version_req(request->payload, payloadLength,
                                        &transferHandle, &transferFlag, &type);

    if (rc != PLDM_SUCCESS)
    {
        encode_get_version_resp(0, rc, 0, 0, nullptr, 4, responsePtr);
        return response;
    }

    ver32_t version{};
    auto search = versions.find(type);

    if (search == versions.end())
    {
        encode_get_version_resp(0, PLDM_ERROR_INVALID_PLDM_TYPE, 0, 0, nullptr,
                                4, responsePtr);
        return response;
    }

    memcpy(&version, &(search->second), sizeof(version));
    encode_get_version_resp(0, PLDM_SUCCESS, 0, PLDM_START_AND_END, &version,
                            sizeof(pldm_version), responsePtr);

    return response;
}

} // namespace responder
} // namespace pldm
