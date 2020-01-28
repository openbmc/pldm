#include "libpldm/base.h"

#include "base.hpp"

#include <array>
#include <cstring>
#include <map>
#include <stdexcept>
#include <vector>

#include "libpldm/bios.h"
#include "libpldm/platform.h"

namespace pldm
{

using Type = uint8_t;

namespace responder
{

using Cmd = std::vector<uint8_t>;

static const std::map<Type, Cmd> capabilities{
    {PLDM_BASE,
     {PLDM_GET_TID, PLDM_GET_PLDM_VERSION, PLDM_GET_PLDM_TYPES,
      PLDM_GET_PLDM_COMMANDS}},
    {PLDM_PLATFORM, {PLDM_SET_STATE_EFFECTER_STATES}},
    {PLDM_BIOS,
     {PLDM_GET_DATE_TIME, PLDM_GET_BIOS_TABLE,
      PLDM_GET_BIOS_ATTRIBUTE_CURRENT_VALUE_BY_HANDLE}},
};

static const std::map<Type, ver32_t> versions{
    {PLDM_BASE, {0xF1, 0xF0, 0xF0, 0x00}},
    {PLDM_PLATFORM, {0xF1, 0xF2, 0xF0, 0x00}},
    {PLDM_BIOS, {0xF1, 0xF0, 0xF0, 0x00}},
    {PLDM_FRU, {0xF1, 0xF0, 0xF0, 0x00}},
};

namespace base
{

Response Handler::getPLDMTypes(const pldm_msg* request,
                               size_t /*payloadLength*/)
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

    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_TYPES_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    auto rc = encode_get_types_resp(request->hdr.instance_id, PLDM_SUCCESS,
                                    types.data(), responsePtr);
    if (rc != PLDM_SUCCESS)
    {
        return CmdHandler::ccOnlyResponse(request, rc);
    }

    return response;
}

Response Handler::getPLDMCommands(const pldm_msg* request, size_t payloadLength)
{
    ver32_t version{};
    Type type;

    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_COMMANDS_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    auto rc = decode_get_commands_req(request, payloadLength, &type, &version);

    if (rc != PLDM_SUCCESS)
    {
        return CmdHandler::ccOnlyResponse(request, rc);
    }

    // DSP0240 has this as a bitfield8[N], where N = 0 to 31
    std::array<bitfield8_t, 32> cmds{};
    if (capabilities.find(type) == capabilities.end())
    {
        return CmdHandler::ccOnlyResponse(request,
                                          PLDM_ERROR_INVALID_PLDM_TYPE);
    }

    for (const auto& cmd : capabilities.at(type))
    {
        auto index = cmd / 8;
        // <Type Number> = <Array Index> * 8 + <bit position>
        auto bit = cmd - (index * 8);
        cmds[index].byte |= 1 << bit;
    }

    rc = encode_get_commands_resp(request->hdr.instance_id, PLDM_SUCCESS,
                                  cmds.data(), responsePtr);
    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    return response;
}

Response Handler::getPLDMVersion(const pldm_msg* request, size_t payloadLength)
{
    uint32_t transferHandle;
    Type type;
    uint8_t transferFlag;

    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_VERSION_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    uint8_t rc = decode_get_version_req(request, payloadLength, &transferHandle,
                                        &transferFlag, &type);

    if (rc != PLDM_SUCCESS)
    {
        return CmdHandler::ccOnlyResponse(request, rc);
    }

    ver32_t version{};
    auto search = versions.find(type);

    if (search == versions.end())
    {
        return CmdHandler::ccOnlyResponse(request,
                                          PLDM_ERROR_INVALID_PLDM_TYPE);
    }

    memcpy(&version, &(search->second), sizeof(version));
    rc = encode_get_version_resp(request->hdr.instance_id, PLDM_SUCCESS, 0,
                                 PLDM_START_AND_END, &version,
                                 sizeof(pldm_version), responsePtr);
    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    return response;
}

Response Handler::getTID(const pldm_msg* request, size_t /*payloadLength*/)
{
    // assigned 1 to the bmc as the PLDM terminus
    uint8_t tid = 1;

    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_TID_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    auto rc = encode_get_tid_resp(request->hdr.instance_id, PLDM_SUCCESS, tid,
                                  responsePtr);
    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    return response;
}

} // namespace base
} // namespace responder
} // namespace pldm
