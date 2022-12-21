#include "pldm_bios_cmd.hpp"

#include "common/bios_utils.hpp"
#include "common/utils.hpp"
#include "pldm_cmd_helper.hpp"

#include <libpldm/bios_table.h>
#include <libpldm/utils.h>

#include <map>
#include <optional>

namespace pldmtool
{

namespace bios
{

namespace
{

using namespace pldmtool::helper;
using namespace pldm::bios::utils;
using namespace pldm::utils;

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

        auto rc = encode_get_date_time_req(instanceId, request);
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

        std::stringstream dt;
        ordered_json data;
        dt << bcd2dec16(year) << "-" << setWidth(month) << "-" << setWidth(day)
           << " " << setWidth(hours) << ":" << setWidth(minutes) << ":"
           << setWidth(seconds);
        data["Response"] = dt.str();
        pldmtool::helper::DisplayInJson(data);
    }

  private:
    static std::string setWidth(uint8_t data)
    {
        std::stringstream s;
        s << std::setfill('0') << std::setw(2)
          << static_cast<uint32_t>(bcd2dec8(data));
        return s.str();
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
            instanceId, seconds, minutes, hours, day, month, year, request,
            sizeof(struct pldm_set_date_time_req));

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

        ordered_json data;
        data["Response"] = "SUCCESS";
        pldmtool::helper::DisplayInJson(data);
    }

  private:
    uint64_t tmData;
};

class GetBIOSTableHandler : public CommandInterface
{
  public:
    ~GetBIOSTableHandler() = default;
    GetBIOSTableHandler() = delete;
    GetBIOSTableHandler(const GetBIOSTableHandler&) = delete;
    GetBIOSTableHandler(GetBIOSTableHandler&&) = delete;
    GetBIOSTableHandler& operator=(const GetBIOSTableHandler&) = delete;
    GetBIOSTableHandler& operator=(GetBIOSTableHandler&&) = delete;

    using Table = std::vector<uint8_t>;

    using CommandInterface::CommandInterface;

    static inline const std::map<pldm_bios_attribute_type, const char*>
        attrTypeMap = {
            {PLDM_BIOS_ENUMERATION, "BIOSEnumeration"},
            {PLDM_BIOS_ENUMERATION_READ_ONLY, "BIOSEnumerationReadOnly"},
            {PLDM_BIOS_STRING, "BIOSString"},
            {PLDM_BIOS_STRING_READ_ONLY, "BIOSStringReadOnly"},
            {PLDM_BIOS_PASSWORD, "BIOSPassword"},
            {PLDM_BIOS_PASSWORD_READ_ONLY, "BIOSPasswordReadOnly"},
            {PLDM_BIOS_INTEGER, "BIOSInteger"},
            {PLDM_BIOS_INTEGER_READ_ONLY, "BIOSIntegerReadOnly"},

        };

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        return {PLDM_ERROR, {}};
    }

    void parseResponseMsg(pldm_msg*, size_t) override
    {}

    std::optional<Table> getBIOSTable(pldm_bios_table_types tableType)
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                        PLDM_GET_BIOS_TABLE_REQ_BYTES);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

        auto rc = encode_get_bios_table_req(instanceId, 0, PLDM_GET_FIRSTPART,
                                            tableType, request);
        if (rc != PLDM_SUCCESS)
        {
            std::cerr << "Encode GetBIOSTable Error, tableType=," << tableType
                      << " ,rc=" << rc << std::endl;
            return std::nullopt;
        }
        std::vector<uint8_t> responseMsg;
        rc = pldmSendRecv(requestMsg, responseMsg);
        if (rc != PLDM_SUCCESS)
        {
            std::cerr << "PLDM: Communication Error, rc =" << rc << std::endl;
            return std::nullopt;
        }

        uint8_t cc = 0, transferFlag = 0;
        uint32_t nextTransferHandle = 0;
        size_t bios_table_offset;
        auto responsePtr =
            reinterpret_cast<struct pldm_msg*>(responseMsg.data());
        auto payloadLength = responseMsg.size() - sizeof(pldm_msg_hdr);

        rc = decode_get_bios_table_resp(responsePtr, payloadLength, &cc,
                                        &nextTransferHandle, &transferFlag,
                                        &bios_table_offset);

        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "GetBIOSTable Response Error: tableType=" << tableType
                      << ", rc=" << rc << ", cc=" << (int)cc << std::endl;
            return std::nullopt;
        }
        auto tableData =
            reinterpret_cast<char*>((responsePtr->payload) + bios_table_offset);
        auto tableSize = payloadLength - sizeof(nextTransferHandle) -
                         sizeof(transferFlag) - sizeof(cc);
        return std::make_optional<Table>(tableData, tableData + tableSize);
    }

    const pldm_bios_attr_table_entry*
        findAttrEntryByName(const std::string& name, const Table& attrTable,
                            const Table& stringTable)
    {
        auto stringEntry = pldm_bios_table_string_find_by_string(
            stringTable.data(), stringTable.size(), name.c_str());
        if (stringEntry == nullptr)
        {
            return nullptr;
        }

        auto nameHandle =
            pldm_bios_table_string_entry_decode_handle(stringEntry);

        for (auto attr : BIOSTableIter<PLDM_BIOS_ATTR_TABLE>(attrTable.data(),
                                                             attrTable.size()))
        {
            auto attrNameHandle =
                pldm_bios_table_attr_entry_decode_string_handle(attr);
            if (attrNameHandle == nameHandle)
            {
                return attr;
            }
        }
        return nullptr;
    }

    std::optional<uint16_t> findAttrHandleByName(const std::string& name,
                                                 const Table& attrTable,
                                                 const Table& stringTable)
    {
        auto attribute = findAttrEntryByName(name, attrTable, stringTable);
        if (attribute == nullptr)
        {
            return std::nullopt;
        }

        return pldm_bios_table_attr_entry_decode_attribute_handle(attribute);
    }

    std::string decodeStringFromStringEntry(
        const pldm_bios_string_table_entry* stringEntry)
    {
        auto strLength =
            pldm_bios_table_string_entry_decode_string_length(stringEntry);
        std::vector<char> buffer(strLength + 1 /* sizeof '\0' */);
        pldm_bios_table_string_entry_decode_string(stringEntry, buffer.data(),
                                                   buffer.size());

        return std::string(buffer.data(), buffer.data() + strLength);
    }

    std::string displayStringHandle(uint16_t handle,
                                    const std::optional<Table>& stringTable,
                                    bool displayHandle = true)
    {
        std::string displayString = std::to_string(handle);
        if (!stringTable)
        {
            return displayString;
        }
        auto stringEntry = pldm_bios_table_string_find_by_handle(
            stringTable->data(), stringTable->size(), handle);
        if (stringEntry == nullptr)
        {
            return displayString;
        }

        auto decodedStr = decodeStringFromStringEntry(stringEntry);
        if (!displayHandle)
        {
            return decodedStr;
        }

        return displayString + "(" + decodedStr + ")";
    }

    std::string displayEnumValueByIndex(uint16_t attrHandle, uint8_t index,
                                        const std::optional<Table>& attrTable,
                                        const std::optional<Table>& stringTable)
    {
        std::string displayString;
        if (!attrTable)
        {
            return displayString;
        }

        auto attrEntry = pldm_bios_table_attr_find_by_handle(
            attrTable->data(), attrTable->size(), attrHandle);
        if (attrEntry == nullptr)
        {
            return displayString;
        }
        auto pvNum = pldm_bios_table_attr_entry_enum_decode_pv_num(attrEntry);
        std::vector<uint16_t> pvHandls(pvNum);
        pldm_bios_table_attr_entry_enum_decode_pv_hdls(
            attrEntry, pvHandls.data(), pvHandls.size());
        return displayStringHandle(pvHandls[index], stringTable, false);
    }

    void displayAttributeValueEntry(
        const pldm_bios_attr_val_table_entry* tableEntry,
        const std::optional<Table>& attrTable,
        const std::optional<Table>& stringTable, bool verbose,
        ordered_json& output)
    {
        auto attrHandle =
            pldm_bios_table_attr_value_entry_decode_attribute_handle(
                tableEntry);
        auto attrType = static_cast<pldm_bios_attribute_type>(
            pldm_bios_table_attr_value_entry_decode_attribute_type(tableEntry));

        if (verbose)
        {
            output["AttributeHandle"] = attrHandle;
            if (attrTypeMap.contains(attrType))
            {
                output["AttributeType"] = attrTypeMap.at(attrType);
            }
            else
            {
                std::cout << "Get AttributeType failed.\n";
            }
        }
        switch (attrType)
        {
            case PLDM_BIOS_ENUMERATION:
            case PLDM_BIOS_ENUMERATION_READ_ONLY:
            {
                auto count =
                    pldm_bios_table_attr_value_entry_enum_decode_number(
                        tableEntry);
                std::vector<uint8_t> handles(count);
                pldm_bios_table_attr_value_entry_enum_decode_handles(
                    tableEntry, handles.data(), handles.size());
                if (verbose)
                {
                    output["NumberOfCurrentValues"] = (int)count;
                }
                for (size_t i = 0; i < handles.size(); i++)
                {
                    if (verbose)
                    {
                        output["CurrentValueStringHandleIndex[" +
                               std::to_string(i) + "]"] =
                            displayEnumValueByIndex(attrHandle, handles[i],
                                                    attrTable, stringTable);
                    }
                    else
                    {
                        output["CurrentValue"] = displayEnumValueByIndex(
                            attrHandle, handles[i], attrTable, stringTable);
                    }
                }
                break;
            }
            case PLDM_BIOS_INTEGER:
            case PLDM_BIOS_INTEGER_READ_ONLY:
            {
                auto cv = pldm_bios_table_attr_value_entry_integer_decode_cv(
                    tableEntry);
                if (verbose)
                {
                    output["CurrentValue"] = cv;
                }
                else
                {
                    output["CurrentValue"] = cv;
                }
                break;
            }
            case PLDM_BIOS_STRING:
            case PLDM_BIOS_STRING_READ_ONLY:
            {
                variable_field currentString;
                pldm_bios_table_attr_value_entry_string_decode_string(
                    tableEntry, &currentString);
                if (verbose)
                {
                    output["CurrentStringLength"] = currentString.length;
                    output["CurrentString"] = std::string(
                        reinterpret_cast<const char*>(currentString.ptr),
                        currentString.length);
                }
                else
                {
                    output["CurrentValue"] = std::string(
                        reinterpret_cast<const char*>(currentString.ptr),
                        currentString.length);
                }

                break;
            }
            case PLDM_BIOS_PASSWORD:
            case PLDM_BIOS_PASSWORD_READ_ONLY:
            {
                std::cout << "Password attribute: Not Supported" << std::endl;
                break;
            }
        }
    }
};

class GetBIOSTable : public GetBIOSTableHandler
{
  public:
    ~GetBIOSTable() = default;
    GetBIOSTable() = delete;
    GetBIOSTable(const GetBIOSTable&) = delete;
    GetBIOSTable(GetBIOSTable&&) = default;
    GetBIOSTable& operator=(const GetBIOSTable&) = delete;
    GetBIOSTable& operator=(GetBIOSTable&&) = default;

    using Table = std::vector<uint8_t>;

    explicit GetBIOSTable(const char* type, const char* name, CLI::App* app) :
        GetBIOSTableHandler(type, name, app)
    {
        app->add_option("-t,--type", pldmBIOSTableType, "pldm bios table type")
            ->required()
            ->transform(
                CLI::CheckedTransformer(pldmBIOSTableTypes, CLI::ignore_case));
    }

    void exec() override
    {
        switch (pldmBIOSTableType)
        {
            case PLDM_BIOS_STRING_TABLE:
            {
                auto stringTable = getBIOSTable(PLDM_BIOS_STRING_TABLE);
                decodeStringTable(stringTable);
                break;
            }
            case PLDM_BIOS_ATTR_TABLE:
            {
                auto stringTable = getBIOSTable(PLDM_BIOS_STRING_TABLE);
                auto attrTable = getBIOSTable(PLDM_BIOS_ATTR_TABLE);

                decodeAttributeTable(attrTable, stringTable);
                break;
            }
            case PLDM_BIOS_ATTR_VAL_TABLE:
            {
                auto stringTable = getBIOSTable(PLDM_BIOS_STRING_TABLE);
                auto attrTable = getBIOSTable(PLDM_BIOS_ATTR_TABLE);
                auto attrValTable = getBIOSTable(PLDM_BIOS_ATTR_VAL_TABLE);

                decodeAttributeValueTable(attrValTable, attrTable, stringTable);
                break;
            }
        }
    }

  private:
    pldm_bios_table_types pldmBIOSTableType;

    void decodeStringTable(const std::optional<Table>& stringTable)
    {
        if (!stringTable)
        {
            std::cerr << "GetBIOSStringTable Error" << std::endl;
            return;
        }
        ordered_json stringdata;

        for (auto tableEntry : BIOSTableIter<PLDM_BIOS_STRING_TABLE>(
                 stringTable->data(), stringTable->size()))
        {
            auto strHandle =
                pldm_bios_table_string_entry_decode_handle(tableEntry);
            auto strTableData = decodeStringFromStringEntry(tableEntry);
            stringdata[std::to_string(strHandle)] = strTableData;
        }
        pldmtool::helper::DisplayInJson(stringdata);
    }
    void decodeAttributeTable(const std::optional<Table>& attrTable,
                              const std::optional<Table>& stringTable)
    {
        if (!stringTable)
        {
            std::cerr << "GetBIOSAttributeTable Error" << std::endl;
            return;
        }
        ordered_json output;

        for (auto entry : BIOSTableIter<PLDM_BIOS_ATTR_TABLE>(
                 attrTable->data(), attrTable->size()))
        {
            ordered_json attrdata;

            auto attrHandle =
                pldm_bios_table_attr_entry_decode_attribute_handle(entry);
            auto attrNameHandle =
                pldm_bios_table_attr_entry_decode_string_handle(entry);
            auto attrType = static_cast<pldm_bios_attribute_type>(
                pldm_bios_table_attr_entry_decode_attribute_type(entry));

            attrdata["AttributeHandle"] = attrHandle;
            attrdata["AttributeNameHandle"] =
                displayStringHandle(attrNameHandle, stringTable);
            if (attrTypeMap.contains(attrType))
            {
                attrdata["AttributeType"] = attrTypeMap.at(attrType);
            }
            else
            {
                std::cout << "Get AttributeType failed.\n";
            }

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

                    attrdata["NumberOfPossibleValues"] = (int)pvNum;

                    for (size_t i = 0; i < pvHandls.size(); i++)
                    {
                        attrdata["PossibleValueStringHandle[" +
                                 std::to_string(i) + "]"] =
                            displayStringHandle(pvHandls[i], stringTable);
                    }
                    attrdata["NumberOfDefaultValues"] = (int)defNum;
                    for (size_t i = 0; i < defIndices.size(); i++)
                    {
                        attrdata["DefaultValueStringHandleIndex[" +
                                 std::to_string(i) + "]"] = (int)defIndices[i];
                        attrdata["DefaultValueStringHandle"] =
                            displayStringHandle(pvHandls[defIndices[i]],
                                                stringTable);
                    }
                    break;
                }
                case PLDM_BIOS_INTEGER:
                case PLDM_BIOS_INTEGER_READ_ONLY:
                {
                    uint64_t lower, upper, def;
                    uint32_t scalar;
                    pldm_bios_table_attr_entry_integer_decode(
                        entry, &lower, &upper, &scalar, &def);
                    attrdata["LowerBound"] = lower;
                    attrdata["UpperBound"] = upper;
                    attrdata["ScalarIncrement"] = scalar;
                    attrdata["DefaultValue"] = def;
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

                    std::stringstream stringtype;
                    stringtype << "0x" << std::hex << std::setw(2)
                               << std::setfill('0') << (int)strType << std::dec
                               << std::setw(0);
                    attrdata["StringType"] = stringtype.str();
                    attrdata["MinimumStringLength"] = (int)min;
                    attrdata["MaximumStringLength"] = (int)max;
                    attrdata["DefaultStringLength"] = (int)def;
                    attrdata["DefaultString"] = defString.data();
                    break;
                }
                case PLDM_BIOS_PASSWORD:
                case PLDM_BIOS_PASSWORD_READ_ONLY:
                    std::cout << "Password attribute: Not Supported"
                              << std::endl;
            }
            output.emplace_back(std::move(attrdata));
        }
        pldmtool::helper::DisplayInJson(output);
    }
    void decodeAttributeValueTable(const std::optional<Table>& attrValTable,
                                   const std::optional<Table>& attrTable,
                                   const std::optional<Table>& stringTable)
    {
        if (!attrValTable)
        {
            std::cerr << "GetBIOSAttributeValueTable Error" << std::endl;
            return;
        }
        ordered_json output;
        for (auto tableEntry : BIOSTableIter<PLDM_BIOS_ATTR_VAL_TABLE>(
                 attrValTable->data(), attrValTable->size()))
        {
            ordered_json attrValueData;
            displayAttributeValueEntry(tableEntry, attrTable, stringTable, true,
                                       attrValueData);
            output.emplace_back(attrValueData);
        }
        pldmtool::helper::DisplayInJson(output);
    }
};

class GetBIOSAttributeCurrentValueByHandle : public GetBIOSTableHandler
{
  public:
    ~GetBIOSAttributeCurrentValueByHandle() = default;
    GetBIOSAttributeCurrentValueByHandle(
        const GetBIOSAttributeCurrentValueByHandle&) = delete;
    GetBIOSAttributeCurrentValueByHandle(
        GetBIOSAttributeCurrentValueByHandle&&) = delete;
    GetBIOSAttributeCurrentValueByHandle&
        operator=(const GetBIOSAttributeCurrentValueByHandle&) = delete;
    GetBIOSAttributeCurrentValueByHandle&
        operator=(GetBIOSAttributeCurrentValueByHandle&&) = delete;

    explicit GetBIOSAttributeCurrentValueByHandle(const char* type,
                                                  const char* name,
                                                  CLI::App* app) :
        GetBIOSTableHandler(type, name, app)
    {
        app->add_option("-a, --attribute", attrName, "pldm BIOS attribute name")
            ->required();
    }

    void exec()
    {
        auto stringTable = getBIOSTable(PLDM_BIOS_STRING_TABLE);
        auto attrTable = getBIOSTable(PLDM_BIOS_ATTR_TABLE);

        if (!stringTable || !attrTable)
        {
            std::cout << "StringTable/AttrTable Unavaliable" << std::endl;
            return;
        }

        auto handle = findAttrHandleByName(attrName, *attrTable, *stringTable);
        if (!handle)
        {
            std::cerr << "Can not find the attribute " << attrName << std::endl;
            return;
        }

        std::vector<uint8_t> requestMsg(
            sizeof(pldm_msg_hdr) +
            PLDM_GET_BIOS_ATTR_CURR_VAL_BY_HANDLE_REQ_BYTES);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

        auto rc = encode_get_bios_attribute_current_value_by_handle_req(
            instanceId, 0, PLDM_GET_FIRSTPART, *handle, request);
        if (rc != PLDM_SUCCESS)
        {
            std::cerr << "PLDM: Request Message Error, rc =" << rc << std::endl;
            return;
        }

        std::vector<uint8_t> responseMsg;
        rc = pldmSendRecv(requestMsg, responseMsg);
        if (rc != PLDM_SUCCESS)
        {
            std::cerr << "PLDM: Communication Error, rc =" << rc << std::endl;
            return;
        }

        uint8_t cc = 0, transferFlag = 0;
        uint32_t nextTransferHandle = 0;
        struct variable_field attributeData;
        auto responsePtr =
            reinterpret_cast<struct pldm_msg*>(responseMsg.data());
        auto payloadLength = responseMsg.size() - sizeof(pldm_msg_hdr);

        rc = decode_get_bios_attribute_current_value_by_handle_resp(
            responsePtr, payloadLength, &cc, &nextTransferHandle, &transferFlag,
            &attributeData);
        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << std::endl;
            return;
        }

        auto tableEntry =
            reinterpret_cast<const struct pldm_bios_attr_val_table_entry*>(
                attributeData.ptr);

        ordered_json avdata;
        displayAttributeValueEntry(tableEntry, attrTable, stringTable, false,
                                   avdata);
        pldmtool::helper::DisplayInJson(avdata);
    }

  private:
    std::string attrName;
};

class SetBIOSAttributeCurrentValue : public GetBIOSTableHandler
{
  public:
    ~SetBIOSAttributeCurrentValue() = default;
    SetBIOSAttributeCurrentValue() = delete;
    SetBIOSAttributeCurrentValue(const SetBIOSAttributeCurrentValue&) = delete;
    SetBIOSAttributeCurrentValue(SetBIOSAttributeCurrentValue&&) = default;
    SetBIOSAttributeCurrentValue&
        operator=(const SetBIOSAttributeCurrentValue&) = delete;
    SetBIOSAttributeCurrentValue&
        operator=(SetBIOSAttributeCurrentValue&&) = default;

    explicit SetBIOSAttributeCurrentValue(const char* type, const char* name,
                                          CLI::App* app) :
        GetBIOSTableHandler(type, name, app)
    {
        app->add_option("-a, --attribute", attrName, "pldm attribute name")
            ->required();
        app->add_option("-d, --data", attrValue, "pldm attribute value")
            ->required();
        // -v is conflict with --verbose in class CommandInterface, so used -d
    }

    void exec()
    {
        auto stringTable = getBIOSTable(PLDM_BIOS_STRING_TABLE);
        auto attrTable = getBIOSTable(PLDM_BIOS_ATTR_TABLE);
        auto attrValueTable = getBIOSTable(PLDM_BIOS_ATTR_VAL_TABLE);

        if (!stringTable || !attrTable)
        {
            std::cout << "StringTable/AttrTable Unavaliable" << std::endl;
            return;
        }

        auto attrEntry =
            findAttrEntryByName(attrName, *attrTable, *stringTable);
        if (attrEntry == nullptr)
        {
            std::cout << "Could not find attribute :" << attrName << std::endl;
            return;
        }

        std::vector<uint8_t> requestMsg;

        int rc = 0;
        auto attrType = attrEntry->attr_type;
        size_t entryLength = 1;
        std::vector<uint8_t> attrValueEntry(entryLength, 0);

        switch (attrType)
        {
            case PLDM_BIOS_ENUMERATION:
            case PLDM_BIOS_ENUMERATION_READ_ONLY:
            {
                entryLength =
                    pldm_bios_table_attr_value_entry_encode_enum_length(1);
                auto pvNum =
                    pldm_bios_table_attr_entry_enum_decode_pv_num(attrEntry);
                std::vector<uint16_t> pvHdls(pvNum, 0);
                pldm_bios_table_attr_entry_enum_decode_pv_hdls(
                    attrEntry, pvHdls.data(), pvNum);
                auto stringEntry = pldm_bios_table_string_find_by_string(
                    stringTable->data(), stringTable->size(),
                    attrValue.c_str());
                if (stringEntry == nullptr)
                {
                    std::cout
                        << "Set Attribute Error: It's not a possible value"
                        << std::endl;
                    return;
                }
                auto valueHandle =
                    pldm_bios_table_string_entry_decode_handle(stringEntry);

                uint8_t i;
                for (i = 0; i < pvNum; i++)
                {
                    if (valueHandle == pvHdls[i])
                        break;
                }
                if (i == pvNum)
                {
                    std::cout
                        << "Set Attribute Error: It's not a possible value"
                        << std::endl;
                    return;
                }

                attrValueEntry.resize(entryLength);
                std::vector<uint8_t> handles = {i};
                pldm_bios_table_attr_value_entry_encode_enum(
                    attrValueEntry.data(), attrValueEntry.size(),
                    attrEntry->attr_handle, attrType, 1, handles.data());
                break;
            }
            case PLDM_BIOS_STRING:
            case PLDM_BIOS_STRING_READ_ONLY:
            {
                entryLength =
                    pldm_bios_table_attr_value_entry_encode_string_length(
                        attrValue.size());

                attrValueEntry.resize(entryLength);

                pldm_bios_table_attr_value_entry_encode_string(
                    attrValueEntry.data(), entryLength, attrEntry->attr_handle,
                    attrType, attrValue.size(), attrValue.c_str());
                break;
            }
            case PLDM_BIOS_INTEGER:
            case PLDM_BIOS_INTEGER_READ_ONLY:
            {
                uint64_t value = std::stoll(attrValue);
                entryLength =
                    pldm_bios_table_attr_value_entry_encode_integer_length();
                attrValueEntry.resize(entryLength);
                pldm_bios_table_attr_value_entry_encode_integer(
                    attrValueEntry.data(), entryLength, attrEntry->attr_handle,
                    attrType, value);
                break;
            }
        }

        requestMsg.resize(entryLength + sizeof(pldm_msg_hdr) +
                          PLDM_SET_BIOS_ATTR_CURR_VAL_MIN_REQ_BYTES);

        rc = encode_set_bios_attribute_current_value_req(
            instanceId, 0, PLDM_START_AND_END, attrValueEntry.data(),
            attrValueEntry.size(),
            reinterpret_cast<pldm_msg*>(requestMsg.data()),
            requestMsg.size() - sizeof(pldm_msg_hdr));

        if (rc != PLDM_SUCCESS)
        {
            std::cerr << "PLDM: Request Message Error, rc =" << rc << std::endl;
            return;
        }
        std::vector<uint8_t> responseMsg;
        rc = pldmSendRecv(requestMsg, responseMsg);
        if (rc != PLDM_SUCCESS)
        {
            std::cerr << "PLDM: Communication Error, rc =" << rc << std::endl;
            return;
        }
        uint8_t cc = 0;
        uint32_t nextTransferHandle = 0;
        auto responsePtr =
            reinterpret_cast<struct pldm_msg*>(responseMsg.data());
        auto payloadLength = responseMsg.size() - sizeof(pldm_msg_hdr);

        rc = decode_set_bios_attribute_current_value_resp(
            responsePtr, payloadLength, &cc, &nextTransferHandle);
        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << std::endl;
            return;
        }

        ordered_json data;
        data["Response"] = "SUCCESS";
        pldmtool::helper::DisplayInJson(data);
    }

  private:
    std::string attrName;
    std::string attrValue;
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

    auto getBIOSAttributeCurrentValueByHandle =
        bios->add_subcommand("GetBIOSAttributeCurrentValueByHandle",
                             "get bios attribute current value by handle");
    commands.push_back(std::make_unique<GetBIOSAttributeCurrentValueByHandle>(
        "bios", "GetBIOSAttributeCurrentValueByHandle",
        getBIOSAttributeCurrentValueByHandle));

    auto setBIOSAttributeCurrentValue = bios->add_subcommand(
        "SetBIOSAttributeCurrentValue", "set bios attribute current value");
    commands.push_back(std::make_unique<SetBIOSAttributeCurrentValue>(
        "bios", "SetBIOSAttributeCurrentValue", setBIOSAttributeCurrentValue));
}

} // namespace bios

} // namespace pldmtool
