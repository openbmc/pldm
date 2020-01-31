#include "pldm_bios_cmd.hpp"

#include "pldm_cmd_helper.hpp"

#include "libpldm/bios_table.h"
#include "libpldm/utils.h"

namespace pldmtool
{

namespace bios
{

namespace
{

using namespace pldmtool::helper;

std::vector<std::unique_ptr<CommandInterface>> commands;

const std::map<const char*, pldm_bios_table_types> pldmBIOSTableTypes{
    {"StringTable", PLDM_BIOS_STRING_TABLE},
    {"AttributeTable", PLDM_BIOS_ATTR_TABLE},
    {"AttributeValueTable", PLDM_BIOS_ATTR_VAL_TABLE},
};

} // namespace

class GetDateTime : public CommandInterface
{
  public:
    ~GetDateTime() = default;
    GetDateTime() = delete;
    GetDateTime(const GetDateTime&) = delete;
    GetDateTime(GetDateTime&&) = default;
    GetDateTime& operator=(const GetDateTime&) = delete;
    GetDateTime& operator=(GetDateTime&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr));
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

        auto rc = encode_get_date_time_req(PLDM_LOCAL_INSTANCE_ID, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;

        uint8_t seconds, minutes, hours, day, month;
        uint16_t year;
        auto rc =
            decode_get_date_time_resp(responsePtr, payloadLength, &cc, &seconds,
                                      &minutes, &hours, &day, &month, &year);
        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << std::endl;
            return;
        }
        std::cout << "Date & Time : " << std::endl;
        std::cout << "YYYY-MM-DD HH:MM:SS - ";
        std::cout << bcd2dec16(year);
        std::cout << "-";
        setWidth(month);
        std::cout << "-";
        setWidth(day);
        std::cout << " ";
        setWidth(hours);
        std::cout << ":";
        setWidth(minutes);
        std::cout << ":";
        setWidth(seconds);
        std::cout << std::endl;
    }

  private:
    void setWidth(uint8_t data)
    {
        std::cout << std::setfill('0') << std::setw(2)
                  << static_cast<uint32_t>(bcd2dec8(data));
    }
};

class SetDateTime : public CommandInterface
{
  public:
    ~SetDateTime() = default;
    SetDateTime() = delete;
    SetDateTime(const SetDateTime&) = delete;
    SetDateTime(SetDateTime&&) = default;
    SetDateTime& operator=(const SetDateTime&) = delete;
    SetDateTime& operator=(SetDateTime&&) = default;

    explicit SetDateTime(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-d,--data", tmData,
                        "set date time data\n"
                        "eg: YYYYMMDDHHMMSS")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                        sizeof(struct pldm_set_date_time_req));
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        uint16_t year = 0;
        uint8_t month = 0;
        uint8_t day = 0;
        uint8_t hours = 0;
        uint8_t minutes = 0;
        uint8_t seconds = 0;

        if (!uintToDate(tmData, &year, &month, &day, &hours, &minutes,
                        &seconds))
        {
            std::cerr << "decode date Error: "
                      << "tmData=" << tmData << std::endl;

            return {PLDM_ERROR_INVALID_DATA, requestMsg};
        }

        auto rc = encode_set_date_time_req(
            PLDM_LOCAL_INSTANCE_ID, seconds, minutes, hours, day, month, year,
            request, sizeof(struct pldm_set_date_time_req));

        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t completionCode = 0;
        auto rc = decode_set_date_time_resp(responsePtr, payloadLength,
                                            &completionCode);

        if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)completionCode
                      << std::endl;
            return;
        }

        std::cout << "SetDateTime: SUCCESS" << std::endl;
    }

  private:
    uint64_t tmData;
};

class GetBIOSTable : public CommandInterface
{
  public:
    ~GetBIOSTable() = default;
    GetBIOSTable() = delete;
    GetBIOSTable(const GetBIOSTable&) = delete;
    GetBIOSTable(GetBIOSTable&&) = default;
    GetBIOSTable& operator=(const GetBIOSTable&) = delete;
    GetBIOSTable& operator=(GetBIOSTable&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetBIOSTable(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-t,--type", pldmBIOSTableType, "pldm bios table type")
            ->required()
            ->transform(
                CLI::CheckedTransformer(pldmBIOSTableTypes, CLI::ignore_case));
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        uint32_t nextTransferHandle = 32;

        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                        PLDM_GET_BIOS_TABLE_REQ_BYTES);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

        auto rc = encode_get_bios_table_req(
            PLDM_LOCAL_INSTANCE_ID, nextTransferHandle, PLDM_GET_FIRSTPART,
            pldmBIOSTableType, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0, transferFlag = 0;
        uint32_t nextTransferHandle = 0;
        size_t bios_table_offset;

        auto rc = decode_get_bios_table_resp(responsePtr, payloadLength, &cc,
                                             &nextTransferHandle, &transferFlag,
                                             &bios_table_offset);

        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << std::endl;
            return;
        }
        auto tableData =
            reinterpret_cast<char*>((responsePtr->payload) + bios_table_offset);
        auto tableSize =
            payloadLength - sizeof(nextTransferHandle) - sizeof(transferFlag);
        printBIOSTable(tableData, tableSize, pldmBIOSTableType);
    }

  private:
    pldm_bios_table_types pldmBIOSTableType;

    void decodeStringTable(const void* tableData, size_t tableSize)
    {
        std::cout << "Parsed Response Msg: " << std::endl;
        std::cout << "BIOSStringHandle : BIOSString" << std::endl;
        std::string strTableData;
        std::unique_ptr<pldm_bios_table_iter,
                        decltype(&pldm_bios_table_iter_free)>
            iter(pldm_bios_table_iter_create(tableData, tableSize,
                                             PLDM_BIOS_STRING_TABLE),
                 pldm_bios_table_iter_free);
        while (!pldm_bios_table_iter_is_end(iter.get()))
        {
            try
            {
                auto tableEntry =
                    pldm_bios_table_iter_string_entry_value(iter.get());
                auto strLength =
                    pldm_bios_table_string_entry_decode_string_length(
                        tableEntry);
                strTableData.resize(strLength + 1);
                auto strHandle =
                    pldm_bios_table_string_entry_decode_handle(tableEntry);
                pldm_bios_table_string_entry_decode_string(
                    tableEntry, strTableData.data(), strTableData.size());
                std::cout << strHandle << " : " << strTableData << std::endl;
            }
            catch (const std::exception& e)
            {
                std::cerr
                    << "handler fails when traversing BIOSStringTable, ERROR="
                    << e.what() << "\n";
            }
            pldm_bios_table_iter_next(iter.get());
        }
    }
    void printBIOSTable(const void* tableData, size_t tableSize,
                        enum pldm_bios_table_types type)
    {
        if (!tableSize)
        {
            std::cerr << "Found table size null." << std::endl;
            return;
        }
        switch (type)
        {
            case PLDM_BIOS_STRING_TABLE:
                decodeStringTable(tableData, tableSize);
                break;
            default:
                break;
        }
    }
};

void registerCommand(CLI::App& app)
{
    auto bios = app.add_subcommand("bios", "bios type command");
    bios->require_subcommand(1);
    auto getDateTime = bios->add_subcommand("GetDateTime", "get date time");
    commands.push_back(
        std::make_unique<GetDateTime>("bios", "GetDateTime", getDateTime));

    auto setDateTime =
        bios->add_subcommand("SetDateTime", "set host date time");
    commands.push_back(
        std::make_unique<SetDateTime>("bios", "setDateTime", setDateTime));

    auto getBIOSTable = bios->add_subcommand("GetBIOSTable", "get bios table");
    commands.push_back(
        std::make_unique<GetBIOSTable>("bios", "GetBIOSTable", getBIOSTable));
}

} // namespace bios

} // namespace pldmtool
