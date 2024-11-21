#include "pldm_base_cmd.hpp"

#include "pldm_cmd_helper.hpp"

#include <libpldm/utils.h>

#ifdef OEM_IBM
#include <libpldm/oem/ibm/file_io.h>
#include <libpldm/oem/ibm/host.h>
#endif

#include <string>

namespace pldmtool
{

namespace base
{

namespace
{

using namespace pldmtool::helper;

std::vector<std::unique_ptr<CommandInterface>> commands;
const std::map<const char*, pldm_supported_types> pldmTypes{
    {"base", PLDM_BASE},   {"platform", PLDM_PLATFORM},
    {"bios", PLDM_BIOS},   {"fru", PLDM_FRU},
#ifdef OEM_IBM
    {"oem-ibm", PLDM_OEM},
#endif
};

const std::map<const char*, pldm_supported_commands> pldmBaseCmds{
    {"SetTID", PLDM_SET_TID},
    {"GetTID", PLDM_GET_TID},
    {"GetPLDMVersion", PLDM_GET_PLDM_VERSION},
    {"GetPLDMTypes", PLDM_GET_PLDM_TYPES},
    {"GetPLDMCommands", PLDM_GET_PLDM_COMMANDS},
    {"SelectPLDMVersion", PLDM_SELECT_PLDM_VERSION},
    {"NegotiateTransferParameters", PLDM_NEGOTIATE_TRANSFER_PARAMETERS},
    {"MultipartSend", PLDM_MULTIPART_SEND},
    {"MultipartReceive", PLDM_MULTIPART_RECEIVE},
    {"GetMultipartTransferSupport", PLDM_GET_MULTIPART_TRANSFER_SUPPORT}};

const std::map<const char*, pldm_bios_commands> pldmBiosCmds{
    {"GetBIOSTable", PLDM_GET_BIOS_TABLE},
    {"SetBIOSTable", PLDM_SET_BIOS_TABLE},
    {"SetBIOSAttributeCurrentValue", PLDM_SET_BIOS_ATTRIBUTE_CURRENT_VALUE},
    {"GetBIOSAttributeCurrentValueByHandle",
     PLDM_GET_BIOS_ATTRIBUTE_CURRENT_VALUE_BY_HANDLE},
    {"GetDateTime", PLDM_GET_DATE_TIME},
    {"SetDateTime", PLDM_SET_DATE_TIME}};

const std::map<const char*, pldm_platform_commands> pldmPlatformCmds{
    {"GetTerminusUID", PLDM_GET_TERMINUS_UID},
    {"SetEventReceiver", PLDM_SET_EVENT_RECEIVER},
    {"GetEventReceiver", PLDM_GET_EVENT_RECEIVER},
    {"PlatformEventMessage", PLDM_PLATFORM_EVENT_MESSAGE},
    {"PollForPlatformEventMessage", PLDM_POLL_FOR_PLATFORM_EVENT_MESSAGE},
    {"EventMessageSupported", PLDM_EVENT_MESSAGE_SUPPORTED},
    {"EventMessageBufferSize", PLDM_EVENT_MESSAGE_BUFFER_SIZE},
    {"SetNumericSensorEnable", PLDM_SET_NUMERIC_SENSOR_ENABLE},
    {"GetSensorReading", PLDM_GET_SENSOR_READING},
    {"GetSensorThresholds", PLDM_GET_SENSOR_THRESHOLDS},
    {"SetSensorThresholds", PLDM_SET_SENSOR_THRESHOLDS},
    {"RestoreSensorThresholds", PLDM_RESTORE_SENSOR_THRESHOLDS},
    {"GetSensorHysteresis", PLDM_GET_SENSOR_HYSTERESIS},
    {"SetSensorHysteresis", PLDM_SET_SENSOR_HYSTERESIS},
    {"InitNumericSensor", PLDM_INIT_NUMERIC_SENSOR},
    {"SetStateSensorEnables", PLDM_SET_STATE_SENSOR_ENABLES},
    {"GetStateSensorReadings", PLDM_GET_STATE_SENSOR_READINGS},
    {"InitStateSensor", PLDM_INIT_STATE_SENSOR},
    {"SetNumericEffecterEnable", PLDM_SET_NUMERIC_EFFECTER_ENABLE},
    {"SetNumericEffecterValue", PLDM_SET_NUMERIC_EFFECTER_VALUE},
    {"GetNumericEffecterValue", PLDM_GET_NUMERIC_EFFECTER_VALUE},
    {"SetStateEffecterEnables", PLDM_SET_STATE_EFFECTER_ENABLES},
    {"SetStateEffecterStates", PLDM_SET_STATE_EFFECTER_STATES},
    {"GetStateEffecterStates", PLDM_GET_STATE_EFFECTER_STATES},
    {"GetPLDMEventLogInfo", PLDM_GET_PLDM_EVENT_LOG_INFO},
    {"EnablePLDMEventLogging", PLDM_ENABLE_PLDM_EVENT_LOGGING},
    {"ClearPLDMEventLog", PLDM_CLEAR_PLDM_EVENT_LOG},
    {"GetPLDMEventLogTimestamp", PLDM_GET_PLDM_EVENT_LOG_TIMESTAMP},
    {"SetPLDMEventLogTimestamp", PLDM_SET_PLDM_EVENT_LOG_TIMESTAMP},
    {"ReadPLDMEventLog", PLDM_READ_PLDM_EVENT_LOG},
    {"GetPLDMEventLogPolicyInfo", PLDM_GET_PLDM_EVENT_LOG_POLICY_INFO},
    {"SetPLDMEventLogPolicy", PLDM_SET_PLDM_EVENT_LOG_POLICY},
    {"FindPLDMEventLogEntry", PLDM_FIND_PLDM_EVENT_LOG_ENTRY},
    {"GetPDRRepositoryInfo", PLDM_GET_PDR_REPOSITORY_INFO},
    {"GetPDR", PLDM_GET_PDR},
    {"FindPDR", PLDM_FIND_PDR},
    {"RunInitAgent", PLDM_RUN_INIT_AGENT},
    {"GetPDRRepositorySignature", PLDM_GET_PDR_REPOSITORY_SIGNATURE}};

const std::map<const char*, pldm_fru_commands> pldmFruCmds{
    {"GetFRURecordTableMetadata", PLDM_GET_FRU_RECORD_TABLE_METADATA},
    {"GetFRURecordTable", PLDM_GET_FRU_RECORD_TABLE},
    {"GetFRURecordByOption", PLDM_GET_FRU_RECORD_BY_OPTION}};

#ifdef OEM_IBM
const std::map<const char*, pldm_host_commands> pldmIBMHostCmds{
    {"GetAlertStatus", PLDM_HOST_GET_ALERT_STATUS}};

const std::map<const char*, pldm_fileio_commands> pldmIBMFileIOCmds{
    {"GetFileTable", PLDM_GET_FILE_TABLE},
    {"ReadFile", PLDM_READ_FILE},
    {"WriteFile", PLDM_WRITE_FILE},
    {"ReadFileInToMemory", PLDM_READ_FILE_INTO_MEMORY},
    {"WriteFileFromMemory", PLDM_WRITE_FILE_FROM_MEMORY},
    {"ReadFileByTypeIntoMemory", PLDM_READ_FILE_BY_TYPE_INTO_MEMORY},
    {"WriteFileByTypeFromMemory", PLDM_WRITE_FILE_BY_TYPE_FROM_MEMORY},
    {"NewFileAvailable", PLDM_NEW_FILE_AVAILABLE},
    {"ReadFileByType", PLDM_READ_FILE_BY_TYPE},
    {"WriteFileByType", PLDM_WRITE_FILE_BY_TYPE},
    {"FileAck", PLDM_FILE_ACK}};
#endif

} // namespace

class GetPLDMTypes : public CommandInterface
{
  public:
    ~GetPLDMTypes() = default;
    GetPLDMTypes() = delete;
    GetPLDMTypes(const GetPLDMTypes&) = delete;
    GetPLDMTypes(GetPLDMTypes&&) = default;
    GetPLDMTypes& operator=(const GetPLDMTypes&) = delete;
    GetPLDMTypes& operator=(GetPLDMTypes&&) = delete;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr));
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        auto rc = encode_get_types_req(instanceId, request);
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
        ordered_json data;
        ordered_json jarray;
        for (int i = 0; i < PLDM_MAX_TYPES; i++)
        {
            bitfield8_t b = types[i / 8];
            if (b.byte & (1 << i % 8))
            {
                auto it = std::find_if(
                    pldmTypes.begin(), pldmTypes.end(),
                    [i](const auto& typePair) { return typePair.second == i; });
                if (it != pldmTypes.end())
                {
                    jarray["PLDM Type"] = it->first;
                    jarray["PLDM Type Code"] = i;
                    data.emplace_back(jarray);
                }
            }
        }
        pldmtool::helper::DisplayInJson(data);
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
    GetPLDMVersion& operator=(GetPLDMVersion&&) = delete;

    explicit GetPLDMVersion(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-t,--type", pldmType, "pldm supported type")
            ->required()
            ->transform(CLI::CheckedTransformer(pldmTypes, CLI::ignore_case));
    }
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(pldm_msg_hdr) + PLDM_GET_VERSION_REQ_BYTES);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

        auto rc = encode_get_version_req(instanceId, 0, PLDM_GET_FIRSTPART,
                                         pldmType, request);
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
        ordered_json data;
        auto it = std::find_if(
            pldmTypes.begin(), pldmTypes.end(),
            [&](const auto& typePair) { return typePair.second == pldmType; });

        if (it != pldmTypes.end())
        {
            data["Response"] = buffer;
        }
        pldmtool::helper::DisplayInJson(data);
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
    GetTID& operator=(GetTID&&) = delete;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr));
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        auto rc = encode_get_tid_req(instanceId, request);
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
        ordered_json data;
        data["Response"] = static_cast<uint32_t>(tid);
        pldmtool::helper::DisplayInJson(data);
    }
};

class GetPLDMCommands : public CommandInterface
{
  public:
    ~GetPLDMCommands() = default;
    GetPLDMCommands() = delete;
    GetPLDMCommands(const GetPLDMCommands&) = delete;
    GetPLDMCommands(GetPLDMCommands&&) = default;
    GetPLDMCommands& operator=(const GetPLDMCommands&) = delete;
    GetPLDMCommands& operator=(GetPLDMCommands&&) = delete;

    explicit GetPLDMCommands(const char* type, const char* name,
                             CLI::App* app) : CommandInterface(type, name, app)
    {
        app->add_option("-t,--type", pldmType, "pldm supported type")
            ->required()
            ->transform(CLI::CheckedTransformer(pldmTypes, CLI::ignore_case));
        app->add_option(
            "-d,--data", inputVersion,
            "Set PLDM type version. Which is got from GetPLDMVersion\n"
            "eg: version 1.1.0 then data will be `0xf1 0xf1 0xf0 0x00`");
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(pldm_msg_hdr) + PLDM_GET_COMMANDS_REQ_BYTES);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        ver32_t version{0xFF, 0xFF, 0xFF, 0xFF};
        if (inputVersion.size() != 0)
        {
            if (inputVersion.size() != 4)
            {
                std::cerr << "Invalid version format. "
                          << "\n";
            }
            else
            {
                version.major = inputVersion[3];
                version.minor = inputVersion[2];
                version.update = inputVersion[1];
                version.alpha = inputVersion[0];
            }
        }
        auto rc =
            encode_get_commands_req(instanceId, pldmType, version, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;
        std::vector<bitfield8_t> cmdTypes(32);
        auto rc = decode_get_commands_resp(responsePtr, payloadLength, &cc,
                                           cmdTypes.data());
        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << "\n";
            return;
        }
        printPldmCommands(cmdTypes, pldmType);
    }

  private:
    pldm_supported_types pldmType;
    std::vector<uint8_t> inputVersion;

    template <typename CommandMap>
    void printCommand(CommandMap& commandMap, int i, ordered_json& jarray)
    {
        auto it = std::find_if(
            commandMap.begin(), commandMap.end(),
            [i](const auto& typePair) { return typePair.second == i; });
        if (it != commandMap.end())
        {
            jarray["PLDM Command Code"] = i;
            jarray["PLDM Command"] = it->first;
        }
    }

    void printPldmCommands(std::vector<bitfield8_t>& cmdTypes,
                           pldm_supported_types pldmType)
    {
        ordered_json output;
        for (int i = 0; i < PLDM_MAX_CMDS_PER_TYPE; i++)
        {
            ordered_json cmdinfo;
            bitfield8_t b = cmdTypes[i / 8];
            if (b.byte & (1 << i % 8))
            {
                switch (pldmType)
                {
                    case PLDM_BASE:
                        printCommand(pldmBaseCmds, i, cmdinfo);
                        break;
                    case PLDM_PLATFORM:
                        printCommand(pldmPlatformCmds, i, cmdinfo);
                        break;
                    case PLDM_BIOS:
                        printCommand(pldmBiosCmds, i, cmdinfo);
                        break;
                    case PLDM_FRU:
                        printCommand(pldmFruCmds, i, cmdinfo);
                        break;
                    case PLDM_OEM:
#ifdef OEM_IBM
                        printCommand(pldmIBMHostCmds, i, cmdinfo);
                        printCommand(pldmIBMFileIOCmds, i, cmdinfo);
#endif
                        break;
                    default:
                        break;
                }
                output.emplace_back(cmdinfo);
            }
        }
        pldmtool::helper::DisplayInJson(output);
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

    auto getPLDMCommands = base->add_subcommand(
        "GetPLDMCommands", "get supported commands of pldm type");
    commands.push_back(std::make_unique<GetPLDMCommands>(
        "base", "GetPLDMCommands", getPLDMCommands));
}

} // namespace base
} // namespace pldmtool
