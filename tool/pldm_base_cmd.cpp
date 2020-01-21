#include "pldm_base_cmd.hpp"

#include "pldm_cmd_helper.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "libpldm/utils.h"

namespace pldmtool
{

namespace base
{

namespace
{

using namespace pldmtool::helper;

std::vector<std::unique_ptr<CommandInterface>> commands;
const std::map<const char*, pldm_supported_types> pldmTypes{
    {"base", PLDM_BASE}, {"platform", PLDM_PLATFORM}, {"bios", PLDM_BIOS},
    {"fru", PLDM_FRU},   {"oem", PLDM_OEM},
};

} // namespace

class GetPLDMTypes : public CommandInterface
{
  public:
    ~GetPLDMTypes() = default;
    GetPLDMTypes() = delete;
    GetPLDMTypes(const GetPLDMTypes&) = delete;
    GetPLDMTypes(GetPLDMTypes&&) = default;
    GetPLDMTypes& operator=(const GetPLDMTypes&) = delete;
    GetPLDMTypes& operator=(GetPLDMTypes&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr));
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        auto rc = encode_get_types_req(PLDM_LOCAL_INSTANCE_ID, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;
        std::vector<bitfield8_t> types(8);
        auto rc = decode_get_types_resp(responsePtr, payloadLength, &cc,
                                        types.data());
        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << "\n";
            return;
        }

        printPldmTypes(types);
    }

  private:
    void printPldmTypes(std::vector<bitfield8_t>& types)
    {
        std::cout << "Supported types:";
        for (int i = 0; i < PLDM_MAX_TYPES; i++)
        {
            bitfield8_t b = types[i / 8];
            if (b.byte & (1 << i % 8))
            {
                std::cout << " " << i;
                auto it = std::find_if(
                    pldmTypes.begin(), pldmTypes.end(),
                    [i](const auto& typePair) { return typePair.second == i; });
                if (it != pldmTypes.end())
                {

                    std::cout << "(" << it->first << ")";
                }
            }
        }

        std::cout << std::endl;
    }
};

class GetPLDMVersion : public CommandInterface
{
  public:
    ~GetPLDMVersion() = default;
    GetPLDMVersion() = delete;
    GetPLDMVersion(const GetPLDMVersion&) = delete;
    GetPLDMVersion(GetPLDMVersion&&) = default;
    GetPLDMVersion& operator=(const GetPLDMVersion&) = delete;
    GetPLDMVersion& operator=(GetPLDMVersion&&) = default;

    explicit GetPLDMVersion(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-t,--type", pldmType, "pldm supported type")
            ->required()
            ->transform(CLI::CheckedTransformer(pldmTypes, CLI::ignore_case));
    }
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                        PLDM_GET_VERSION_REQ_BYTES);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

        auto rc = encode_get_version_req(PLDM_LOCAL_INSTANCE_ID, 0,
                                         PLDM_GET_FIRSTPART, pldmType, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0, transferFlag = 0;
        uint32_t transferHandle = 0;
        ver32_t version;
        auto rc =
            decode_get_version_resp(responsePtr, payloadLength, &cc,
                                    &transferHandle, &transferFlag, &version);
        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << "\n";
            return;
        }
        char buffer[16] = {0};
        ver2str(&version, buffer, sizeof(buffer));
        std::cout << "Type " << pldmType;
        auto it = std::find_if(
            pldmTypes.begin(), pldmTypes.end(),
            [&](const auto& typePair) { return typePair.second == pldmType; });

        if (it != pldmTypes.end())
        {
            std::cout << "(" << it->first << ")";
        }
        std::cout << ": " << buffer << std::endl;
    }

  private:
    pldm_supported_types pldmType;
};

class GetTID : public CommandInterface
{
  public:
    ~GetTID() = default;
    GetTID() = delete;
    GetTID(const GetTID&) = delete;
    GetTID(GetTID&&) = default;
    GetTID& operator=(const GetTID&) = delete;
    GetTID& operator=(GetTID&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr));
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        auto rc = encode_get_tid_req(PLDM_LOCAL_INSTANCE_ID, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;
        uint8_t tid = 0;
        std::vector<bitfield8_t> types(8);
        auto rc = decode_get_tid_resp(responsePtr, payloadLength, &cc, &tid);
        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << "\n";
            return;
        }
        std::cout << "Parsed Response Msg: " << std::endl;
        std::cout << "TID : " << static_cast<uint32_t>(tid) << std::endl;
    }
};

void registerCommand(CLI::App& app)
{
    auto base = app.add_subcommand("base", "base type command");
    base->require_subcommand(1);

    auto getPLDMTypes =
        base->add_subcommand("GetPLDMTypes", "get pldm supported types");
    commands.push_back(
        std::make_unique<GetPLDMTypes>("base", "GetPLDMTypes", getPLDMTypes));

    auto getPLDMVersion =
        base->add_subcommand("GetPLDMVersion", "get version of a certain type");
    commands.push_back(std::make_unique<GetPLDMVersion>(
        "base", "GetPLDMVersion", getPLDMVersion));

    auto getPLDMTID = base->add_subcommand("GetTID", "get Terminus ID (TID)");
    commands.push_back(std::make_unique<GetTID>("base", "GetTID", getPLDMTID));
}

} // namespace base
} // namespace pldmtool
