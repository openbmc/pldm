#include "pldm_bios_cmd.hpp"

#include "bios_utils.hpp"
#include "pldm_cmd_helper.hpp"
#include "utils.hpp"

#include "libpldm/bios_table.h"
#include "libpldm/utils.h"

namespace pldmtool
{

namespace bios
{

namespace
{

using namespace pldmtool::helper;
using namespace pldm::bios::utils;

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
        auto tableSize = payloadLength - sizeof(nextTransferHandle) -
                         sizeof(transferFlag) - sizeof(cc);
        printBIOSTable(tableData, tableSize, pldmBIOSTableType);
    }

  private:
    pldm_bios_table_types pldmBIOSTableType;

    void decodeStringTable(const void* tableData, size_t tableSize)
    {
        std::cout << "Parsed Response Msg: " << std::endl;
        std::cout << "BIOSStringHandle : BIOSString" << std::endl;

        std::string strTableData;
        for (auto tableEntry :
             BIOSTableIter<PLDM_BIOS_STRING_TABLE>(tableData, tableSize))
        {
            auto strLength =
                pldm_bios_table_string_entry_decode_string_length(tableEntry);
            strTableData.resize(strLength + 1);
            auto strHandle =
                pldm_bios_table_string_entry_decode_handle(tableEntry);
            pldm_bios_table_string_entry_decode_string(
                tableEntry, strTableData.data(), strTableData.size());
            std::cout << strHandle << " : " << strTableData << std::endl;
        }
    }
    void decodeAttributeTable(const void* tableData, size_t tableSize)
    {
        std::cout << "AttributeTable: " << std::endl;
        for (auto entry :
             BIOSTableIter<PLDM_BIOS_ATTR_TABLE>(tableData, tableSize))
        {
            auto attrHandle =
                pldm_bios_table_attr_entry_decode_attribute_handle(entry);
            auto attrNameHandle =
                pldm_bios_table_attr_entry_decode_string_handle(entry);
            auto attrType = static_cast<pldm_bios_attribute_type>(
                pldm_bios_table_attr_entry_decode_attribute_type(entry));
            std::cout << "AttributeHandle: " << attrHandle
                      << ", AttributeNameHandle: " << attrNameHandle
                      << ", AttributeType: 0x" << std::hex << std::setw(2)
                      << std::setfill('0') << attrType << std::dec
                      << std::setw(0) << std::endl;
            switch (attrType)
            {
                case PLDM_BIOS_ENUMERATION:
                case PLDM_BIOS_ENUMERATION_READ_ONLY:
                {
                    auto pvNum =
                        pldm_bios_table_attr_entry_enum_decode_pv_num(entry);
                    std::vector<uint16_t> pvHandls(pvNum);
                    pldm_bios_table_attr_entry_enum_decode_pv_hdls(
                        entry, pvHandls.data(), pvHandls.size());
                    auto defNum =
                        pldm_bios_table_attr_entry_enum_decode_def_num(entry);
                    std::vector<uint8_t> defIndices(defNum);
                    pldm_bios_table_attr_entry_enum_decode_def_indices(
                        entry, defIndices.data(), defIndices.size());
                    std::cout << "\tNumberOfPossibleValues: " << (int)pvNum
                              << std::endl;

                    for (size_t i = 0; i < pvHandls.size(); i++)
                    {
                        std::cout << "\t\tPossibleValueStringHandle"
                                  << "[" << i << "] = " << pvHandls[i]
                                  << std::endl;
                    }
                    std::cout << "\tNumberOfDefaultValues: " << (int)defNum
                              << std::endl;
                    for (size_t i = 0; i < defIndices.size(); i++)
                    {
                        std::cout << "\t\tDefaultValueStringHandleIndex"
                                  << "[" << i << "] = " << (int)defIndices[i]
                                  << std::endl;
                    }
                    std::cout << std::endl;
                    break;
                }
                case PLDM_BIOS_INTEGER:
                case PLDM_BIOS_INTEGER_READ_ONLY:
                {
                    uint64_t lower, upper, def;
                    uint32_t scalar;
                    pldm_bios_table_attr_entry_integer_decode(
                        entry, &lower, &upper, &scalar, &def);
                    std::cout << "\tLowerBound: " << lower << std::endl
                              << "\tUpperBound: " << upper << std::endl
                              << "\tScalarIncrement: " << scalar << std::endl
                              << "\tDefaultValue:" << def << std::endl;
                    break;
                }
                case PLDM_BIOS_STRING:
                case PLDM_BIOS_STRING_READ_ONLY:
                {
                    auto strType =
                        pldm_bios_table_attr_entry_string_decode_string_type(
                            entry);
                    auto min =
                        pldm_bios_table_attr_entry_string_decode_min_length(
                            entry);
                    auto max =
                        pldm_bios_table_attr_entry_string_decode_max_length(
                            entry);
                    auto def =
                        pldm_bios_table_attr_entry_string_decode_def_string_length(
                            entry);
                    std::vector<char> defString(def + 1);
                    pldm_bios_table_attr_entry_string_decode_def_string(
                        entry, defString.data(), defString.size());
                    std::cout
                        << "\tStringType: 0x" << std::hex << std::setw(2)
                        << std::setfill('0') << (int)strType << std::dec
                        << std::setw(0) << std::endl
                        << "\tMinimumStringLength: " << (int)min << std::endl
                        << "\tMaximumStringLength: " << (int)max << std::endl
                        << "\tDefaultStringLength: " << (int)def << std::endl
                        << "\tDefaultString: " << defString.data() << std::endl;
                    break;
                }
                case PLDM_BIOS_PASSWORD:
                case PLDM_BIOS_PASSWORD_READ_ONLY:
                    std::cout << "Password attribute: Not Supported"
                              << std::endl;
            }
        }
    }
    void decodeAttributeValueTable(const void* tableData, size_t tableSize)
    {
        std::cout << "AttributeValueTable: " << std::endl;
        for (auto entry :
             BIOSTableIter<PLDM_BIOS_ATTR_VAL_TABLE>(tableData, tableSize))
        {
            auto attrHandle =
                pldm_bios_table_attr_value_entry_decode_attribute_handle(entry);
            auto attrType = static_cast<pldm_bios_attribute_type>(
                pldm_bios_table_attr_value_entry_decode_attribute_type(entry));
            std::cout << "AttributeHandle: " << attrHandle;
            std::cout << ", AttributeType: "
                      << "0x" << std::setfill('0') << std::hex << std::setw(2)
                      << attrType << std::dec << std::setw(0) << std::endl;
            switch (attrType)
            {
                case PLDM_BIOS_ENUMERATION:
                case PLDM_BIOS_ENUMERATION_READ_ONLY:
                {
                    auto count =
                        pldm_bios_table_attr_value_entry_enum_decode_number(
                            entry);
                    std::vector<uint8_t> handles(count);
                    pldm_bios_table_attr_value_entry_enum_decode_handles(
                        entry, handles.data(), handles.size());
                    std::cout << "\tNumberOfCurrentValues: " << (int)count
                              << std::endl;
                    for (size_t i = 0; i < handles.size(); i++)
                    {
                        std::cout << "\tCurrentValueStringHandleIndex[" << i
                                  << "] = " << (int)handles[i] << std::endl;
                    }
                    break;
                }
                case PLDM_BIOS_INTEGER:
                case PLDM_BIOS_INTEGER_READ_ONLY:
                {
                    auto cv =
                        pldm_bios_table_attr_value_entry_integer_decode_cv(
                            entry);
                    std::cout << "\tCurrentValue: " << cv << std::endl;
                    break;
                }
                case PLDM_BIOS_STRING:
                case PLDM_BIOS_STRING_READ_ONLY:
                {
                    auto stringLength =
                        pldm_bios_table_attr_value_entry_string_decode_length(
                            entry);
                    variable_field currentString;
                    pldm_bios_table_attr_value_entry_string_decode_string(
                        entry, &currentString);
                    std::cout << "\tCurrentStringLength: " << stringLength
                              << std::endl
                              << "\tCurrentString: "
                              << std::string(reinterpret_cast<const char*>(
                                                 currentString.ptr),
                                             currentString.length)
                              << std::endl;

                    break;
                }
                case PLDM_BIOS_PASSWORD:
                case PLDM_BIOS_PASSWORD_READ_ONLY:
                    std::cout << "Password attribute: Not Supported"
                              << std::endl;
            }
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
            case PLDM_BIOS_ATTR_TABLE:
                decodeAttributeTable(tableData, tableSize);
                break;
            case PLDM_BIOS_ATTR_VAL_TABLE:
                decodeAttributeValueTable(tableData, tableSize);
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
