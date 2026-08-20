#include "pldm_fru_cmd.hpp"

#include "pldm_cmd_helper.hpp"

#include <libpldm/edac.h>
#ifdef OEM_IBM
#include <libpldm/oem/ibm/fru.h>
#endif

#include <endian.h>

#include <algorithm>
#include <fstream>
#include <limits>

namespace pldmtool
{

namespace fru
{

namespace
{

using namespace pldmtool::helper;

std::vector<std::unique_ptr<CommandInterface>> commands;

} // namespace

class GetFruRecordTableMetadata : public CommandInterface
{
  public:
    ~GetFruRecordTableMetadata() override = default;
    GetFruRecordTableMetadata() = delete;
    GetFruRecordTableMetadata(const GetFruRecordTableMetadata&) = delete;
    GetFruRecordTableMetadata(GetFruRecordTableMetadata&&) = default;
    GetFruRecordTableMetadata& operator=(const GetFruRecordTableMetadata&) =
        delete;
    GetFruRecordTableMetadata& operator=(GetFruRecordTableMetadata&&) = delete;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr));
        auto request = new (requestMsg.data()) pldm_msg;

        auto rc = encode_get_fru_record_table_metadata_req(
            instanceId, request, PLDM_GET_FRU_RECORD_TABLE_METADATA_REQ_BYTES);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;
        uint8_t fru_data_major_version = 0, fru_data_minor_version = 0;
        uint32_t fru_table_maximum_size = 0, fru_table_length = 0;
        uint16_t total_record_set_identifiers = 0, total_table_records = 0;
        uint32_t checksum = 0;

        auto rc = decode_get_fru_record_table_metadata_resp(
            responsePtr, payloadLength, &cc, &fru_data_major_version,
            &fru_data_minor_version, &fru_table_maximum_size, &fru_table_length,
            &total_record_set_identifiers, &total_table_records, &checksum);
        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << std::endl;
            return;
        }
        ordered_json output;
        output["FRUDATAMajorVersion"] =
            static_cast<uint32_t>(fru_data_major_version);
        output["FRUDATAMinorVersion"] =
            static_cast<uint32_t>(fru_data_minor_version);
        output["FRUTableMaximumSize"] = fru_table_maximum_size;
        output["FRUTableLength"] = fru_table_length;
        output["Total number of Record Set Identifiers in table"] =
            total_record_set_identifiers;
        output["Total number of records in table"] = total_table_records;
        output["FRU DATAStructureTableIntegrityChecksum"] = checksum;
        pldmtool::helper::DisplayInJson(output);
    }
};

class FRUTablePrint
{
  public:
    explicit FRUTablePrint(const uint8_t* table, size_t table_size) :
        table(table), table_size(table_size)
    {}

    void print()
    {
        auto p = table;
        ordered_json frutable;
        ordered_json output;
        while (!isTableEnd(p))
        {
            auto record =
                reinterpret_cast<const pldm_fru_record_data_format*>(p);
            output["FRU Record Set Identifier"] =
                (int)le16toh(record->record_set_id);
            output["FRU Record Type"] =
                typeToString(fruRecordTypes, record->record_type);
            output["Number of FRU fields"] = (int)record->num_fru_fields;
            output["Encoding Type for FRU fields"] =
                typeToString(fruEncodingType, record->encoding_type);

            p += sizeof(pldm_fru_record_data_format) -
                 sizeof(pldm_fru_record_tlv);

            std::map<uint8_t, std::string> FruFieldTypeMap;
            std::string fruFieldValue;

            ordered_json frudata;
            ordered_json frufielddata;
            frufielddata.emplace_back(output);
            for (int i = 0; i < record->num_fru_fields; i++)
            {
                auto tlv = reinterpret_cast<const pldm_fru_record_tlv*>(p);
                if (record->record_type == PLDM_FRU_RECORD_TYPE_GENERAL)
                {
                    FruFieldTypeMap.insert(fruGeneralFieldTypes.begin(),
                                           fruGeneralFieldTypes.end());
                    if (tlv->type == PLDM_FRU_FIELD_TYPE_IANA)
                    {
                        fruFieldValue =
                            fruFieldParserU32(tlv->value, tlv->length);
                    }
                    else if (tlv->type == PLDM_FRU_FIELD_TYPE_MANUFAC_DATE)
                    {
                        fruFieldValue =
                            fruFieldParserTimestamp(tlv->value, tlv->length);
                    }
                    else
                    {
                        fruFieldValue =
                            fruFieldValuestring(tlv->value, tlv->length);
                    }

                    frudata["FRU Field Type"] =
                        typeToString(FruFieldTypeMap, tlv->type);
                    frudata["FRU Field Length"] = (int)(tlv->length);
                    frudata["FRU Field Value"] = fruFieldValue;
                    frufielddata.emplace_back(frudata);
                }
                else
                {
#ifdef OEM_IBM
                    if (tlv->type == PLDM_OEM_FRU_FIELD_TYPE_RT)
                    {
                        auto oemIPZValue =
                            fruFieldValuestring(tlv->value, tlv->length);

                        if (populateMaps.contains(oemIPZValue))
                        {
                            const std::map<uint8_t, std::string> IPZTypes =
                                populateMaps.at(oemIPZValue);
                            FruFieldTypeMap.insert(IPZTypes.begin(),
                                                   IPZTypes.end());
                        }
                    }
                    else
                    {
                        FruFieldTypeMap.insert(fruOemFieldTypes.begin(),
                                               fruOemFieldTypes.end());
                    }
                    if (tlv->type == PLDM_OEM_FRU_FIELD_TYPE_IANA)
                    {
                        fruFieldValue =
                            fruFieldParserU32(tlv->value, tlv->length);
                    }
                    else if (tlv->type != 2)
                    {
                        fruFieldValue =
                            fruFieldIPZParser(tlv->value, tlv->length);
                    }
                    else
                    {
                        fruFieldValue =
                            fruFieldValuestring(tlv->value, tlv->length);
                    }
                    frudata["FRU Field Type"] =
                        typeToString(FruFieldTypeMap, tlv->type);
                    frudata["FRU Field Length"] = (int)(tlv->length);
                    frudata["FRU Field Value"] = fruFieldValue;
                    frufielddata.emplace_back(frudata);

#endif
                }
                p += sizeof(pldm_fru_record_tlv) - 1 + tlv->length;
            }
            frutable.emplace_back(frufielddata);
        }
        pldmtool::helper::DisplayInJson(frutable);
    }

  private:
    const uint8_t* table;
    size_t table_size;

    bool isTableEnd(const uint8_t* p)
    {
        auto offset = p - table;
        return (table_size - offset) <= 7;
    }

    static inline const std::map<uint8_t, std::string> fruEncodingType{
        {PLDM_FRU_ENCODING_UNSPECIFIED, "Unspecified"},
        {PLDM_FRU_ENCODING_ASCII, "ASCII"},
        {PLDM_FRU_ENCODING_UTF8, "UTF8"},
        {PLDM_FRU_ENCODING_UTF16, "UTF16"},
        {PLDM_FRU_ENCODING_UTF16LE, "UTF16LE"},
        {PLDM_FRU_ENCODING_UTF16BE, "UTF16BE"}};

    static inline const std::map<uint8_t, std::string> fruGeneralFieldTypes{
        {PLDM_FRU_FIELD_TYPE_CHASSIS, "Chassis"},
        {PLDM_FRU_FIELD_TYPE_MODEL, "Model"},
        {PLDM_FRU_FIELD_TYPE_PN, "Part Number"},
        {PLDM_FRU_FIELD_TYPE_SN, "Serial Number"},
        {PLDM_FRU_FIELD_TYPE_MANUFAC, "Manufacturer"},
        {PLDM_FRU_FIELD_TYPE_MANUFAC_DATE, "Manufacture Date"},
        {PLDM_FRU_FIELD_TYPE_VENDOR, "Vendor"},
        {PLDM_FRU_FIELD_TYPE_NAME, "Name"},
        {PLDM_FRU_FIELD_TYPE_SKU, "SKU"},
        {PLDM_FRU_FIELD_TYPE_VERSION, "Version"},
        {PLDM_FRU_FIELD_TYPE_ASSET_TAG, "Asset Tag"},
        {PLDM_FRU_FIELD_TYPE_DESC, "Description"},
        {PLDM_FRU_FIELD_TYPE_EC_LVL, "Engineering Change Level"},
        {PLDM_FRU_FIELD_TYPE_OTHER, "Other Information"},
        {PLDM_FRU_FIELD_TYPE_IANA, "Vendor IANA"}};

    static inline const std::map<uint8_t, std::string> fruRecordTypes{
        {PLDM_FRU_RECORD_TYPE_GENERAL, "General"},
        {PLDM_FRU_RECORD_TYPE_OEM, "OEM"}};

#ifdef OEM_IBM
    static inline const std::map<uint8_t, std::string> fruOemFieldTypes{
        {PLDM_OEM_FRU_FIELD_TYPE_IANA, "Vendor IANA"},
        {PLDM_OEM_FRU_FIELD_TYPE_RT, "RT"},
        {PLDM_OEM_FRU_FIELD_TYPE_LOCATION_CODE, "Location Code"}};

    static inline const std::map<uint8_t, std::string> VINIFieldTypes{
        {2, "RT"},  {3, "B3"},  {4, "B4"},  {5, "B7"},  {6, "CC"},  {7, "CE"},
        {8, "CT"},  {9, "DR"},  {10, "FG"}, {11, "FN"}, {12, "HE"}, {13, "HW"},
        {14, "HX"}, {15, "PN"}, {16, "SN"}, {17, "TS"}, {18, "VZ"}};

    static inline const std::map<uint8_t, std::string> VSYSFieldTypes{
        {2, "RT"},  {3, "BR"},  {4, "DR"},  {5, "FV"},  {6, "ID"},
        {7, "MN"},  {8, "NN"},  {9, "RB"},  {10, "RG"}, {11, "SE"},
        {12, "SG"}, {13, "SU"}, {14, "TM"}, {15, "TN"}, {16, "WN"}};

    static inline const std::map<uint8_t, std::string> LXR0FieldTypes{
        {2, "RT"}, {3, "LX"}, {4, "VZ"}};

    static inline const std::map<uint8_t, std::string> VW10FieldTypes{
        {2, "RT"}, {3, "DR"}, {4, "GD"}};

    static inline const std::map<uint8_t, std::string> VR10FieldTypes{
        {2, "RT"}, {3, "DC"}, {4, "DR"}, {5, "FL"}, {6, "WA"}};

    static inline const std::map<std::string,
                                 const std::map<uint8_t, std::string>>
        populateMaps{{"VINI", VINIFieldTypes},
                     {"VSYS", VSYSFieldTypes},
                     {"LXR0", LXR0FieldTypes},
                     {"VWX10", VW10FieldTypes},
                     {"VR10", VR10FieldTypes}};
#endif

    std::string typeToString(std::map<uint8_t, std::string> typeMap,
                             uint8_t type)
    {
        auto typeString = std::to_string(type);
        try
        {
            return std::string(typeMap.at(type)) + "(" + typeString + ")";
        }
        catch (const std::out_of_range& e)
        {
            return typeString;
        }
    }

    std::string fruFieldValuestring(const uint8_t* value, uint8_t length)
    {
        return std::string(reinterpret_cast<const char*>(value), length);
    }

    static std::string fruFieldParserU32(const uint8_t* value, uint8_t length)
    {
        assert(length == 4);
        uint32_t v = 0;
        std::memcpy(&v, value, length);
        return std::to_string(le32toh(v));
    }

    static std::string fruFieldParserTimestamp(const uint8_t*, uint8_t)
    {
        return std::string("TODO");
    }

    static std::string fruFieldIPZParser(const uint8_t* value, uint8_t length)
    {
        std::ostringstream tempStream;
        for (int i = 0; i < int(length); ++i)
        {
            tempStream << "0x" << std::setfill('0') << std::setw(2) << std::hex
                       << (unsigned)value[i] << " ";
        }
        return tempStream.str();
    }
};

class GetFRURecordByOption : public CommandInterface
{
  public:
    ~GetFRURecordByOption() override = default;
    GetFRURecordByOption() = delete;
    GetFRURecordByOption(const GetFRURecordByOption&) = delete;
    GetFRURecordByOption& operator=(const GetFRURecordByOption&) = delete;
    GetFRURecordByOption& operator=(GetFRURecordByOption&&) = delete;

    explicit GetFRURecordByOption(const char* type, const char* name,
                                  CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-i, --identifier", recordSetIdentifier,
                        "Record Set Identifier\n"
                        "Possible values: {All record sets = 0, Specific "
                        "record set = 1 – 65535}")
            ->required();
        app->add_option("-r, --record", recordType,
                        "Record Type\n"
                        "Possible values: {All record types = 0, Specific "
                        "record types = 1 – 255}")
            ->required();
        app->add_option("-f, --field", fieldType,
                        "Field Type\n"
                        "Possible values: {All record field types = 0, "
                        "Specific field types = 1 – 15}")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        if (fieldType != 0 && recordType == 0)
        {
            throw std::invalid_argument("if field type is non-zero, the record "
                                        "type shall also be non-zero");
        }
        if (recordType == 254 && (fieldType > 2 && fieldType < 254))
        {
            throw std::invalid_argument(
                "GetFRURecordByOption is not supported for recordType : 254 "
                "and fieldType > 2");
        }

        auto payloadLength = sizeof(pldm_get_fru_record_by_option_req);

        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) + payloadLength,
                                        0);
        auto reqMsg = new (requestMsg.data()) pldm_msg;
        auto rc = encode_get_fru_record_by_option_req(
            instanceId, 0 /* DataTransferHandle */, 0 /* FRUTableHandle */,
            recordSetIdentifier, recordType, fieldType, PLDM_GET_FIRSTPART,
            reqMsg, payloadLength);

        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;
        uint32_t dataTransferHandle = 0;
        uint8_t transferFlag = 0;
        variable_field fruData;

        auto rc = decode_get_fru_record_by_option_resp(
            responsePtr, payloadLength, &cc, &dataTransferHandle, &transferFlag,
            &fruData);

        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << std::endl;
            return;
        }

        FRUTablePrint tablePrint(fruData.ptr, fruData.length);
        tablePrint.print();
    }

  private:
    uint16_t recordSetIdentifier;
    uint8_t recordType;
    uint8_t fieldType;
};

class GetFruRecordTable : public CommandInterface
{
  public:
    ~GetFruRecordTable() override = default;
    GetFruRecordTable() = delete;
    GetFruRecordTable(const GetFruRecordTable&) = delete;
    GetFruRecordTable(GetFruRecordTable&&) = default;
    GetFruRecordTable& operator=(const GetFruRecordTable&) = delete;
    GetFruRecordTable& operator=(GetFruRecordTable&&) = delete;

    using CommandInterface::CommandInterface;
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES);
        auto request = new (requestMsg.data()) pldm_msg;

        auto rc = encode_get_fru_record_table_req(
            instanceId, 0, PLDM_GET_FIRSTPART, request,
            requestMsg.size() - sizeof(pldm_msg_hdr));
        return {rc, requestMsg};
    }
    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;
        uint32_t next_data_transfer_handle = 0;
        uint8_t transfer_flag = 0;
        size_t fru_record_table_length = 0;
        std::vector<uint8_t> fru_record_table_data(payloadLength);

        auto rc = decode_get_fru_record_table_resp(
            responsePtr, payloadLength, &cc, &next_data_transfer_handle,
            &transfer_flag, fru_record_table_data.data(),
            &fru_record_table_length);

        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << std::endl;
            return;
        }

        FRUTablePrint tablePrint(fru_record_table_data.data(),
                                 fru_record_table_length);
        tablePrint.print();
    }
};

class SetFruRecordTable : public CommandInterface
{
  public:
    ~SetFruRecordTable() = default;
    SetFruRecordTable() = delete;
    SetFruRecordTable(const SetFruRecordTable&) = delete;
    SetFruRecordTable(SetFruRecordTable&&) = default;
    SetFruRecordTable& operator=(const SetFruRecordTable&) = delete;
    SetFruRecordTable& operator=(SetFruRecordTable&&) = delete;

    explicit SetFruRecordTable(const char* type, const char* name,
                               CLI::App* app) :
        CommandInterface(type, name, app), maxTransferSize(0), sentOffset(0),
        curChunkLength(0), dataTransferHandle(0), partsSent(0),
        morePartsToSend(false)
    {
        app->add_option("-f, --file", fruJsonPath,
                        "JSON file describing the FRU record table to set")
            ->required()
            ->check(CLI::ExistingFile);
        app->add_option(
            "-s, --maxsize", maxTransferSize,
            "Maximum FRU table bytes carried per request message. 0 (default) "
            "sends the whole table in a single transfer");
    }

    void exec() override
    {
        try
        {
            fruTable = buildFruRecordTable();
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to build FRU record table from " << fruJsonPath
                      << ": " << e.what() << std::endl;
            return;
        }

        sentOffset = 0;
        dataTransferHandle = 0;
        partsSent = 0;
        do
        {
            // Reset before each request so that a request which fails early
            // in CommandInterface::exec() (before parseResponseMsg runs)
            // terminates the loop instead of spinning forever.
            morePartsToSend = false;
            CommandInterface::exec();
        } while (morePartsToSend);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        const size_t remaining = fruTable.size() - sentOffset;
        curChunkLength =
            (maxTransferSize == 0)
                ? remaining
                : std::min(static_cast<size_t>(maxTransferSize), remaining);

        uint8_t transferFlag = 0;
        if (sentOffset == 0 && curChunkLength == fruTable.size())
        {
            transferFlag = PLDM_START_AND_END;
        }
        else if (sentOffset == 0)
        {
            transferFlag = PLDM_START;
        }
        else if (curChunkLength == remaining)
        {
            transferFlag = PLDM_END;
        }
        else
        {
            transferFlag = PLDM_MIDDLE;
        }

        const size_t payloadLength =
            PLDM_SET_FRU_RECORD_TABLE_MIN_REQ_BYTES + curChunkLength;
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) + payloadLength);
        auto request = new (requestMsg.data()) pldm_msg;

        auto rc = encode_set_fru_record_table_req(
            instanceId, dataTransferHandle, transferFlag,
            fruTable.data() + sentOffset, curChunkLength, request,
            payloadLength);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;
        uint32_t nextDataTransferHandle = 0;

        auto rc = decode_set_fru_record_table_resp(
            responsePtr, payloadLength, &cc, &nextDataTransferHandle);
        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << std::endl;
            morePartsToSend = false;
            return;
        }

        sentOffset += curChunkLength;
        dataTransferHandle = nextDataTransferHandle;
        partsSent++;

        if (sentOffset < fruTable.size())
        {
            morePartsToSend = true;
            return;
        }

        morePartsToSend = false;
        ordered_json output;
        output["Result"] = "Success";
        output["FRUTableLength"] = fruTable.size();
        output["NumberOfPartsSent"] = partsSent;
        output["NextDataTransferHandle"] = nextDataTransferHandle;
        pldmtool::helper::DisplayInJson(output);
    }

  private:
    static uint8_t parseRecordType(const nlohmann::json& value)
    {
        if (value.is_string())
        {
            const auto name = value.get<std::string>();
            if (name == "General")
            {
                return PLDM_FRU_RECORD_TYPE_GENERAL;
            }
            if (name == "OEM")
            {
                return PLDM_FRU_RECORD_TYPE_OEM;
            }
            throw std::invalid_argument(
                "RecordType must be \"General\", \"OEM\", or a numeric value");
        }
        return value.get<uint8_t>();
    }

    static uint8_t parseEncodingType(const nlohmann::json& value)
    {
        static const std::map<std::string, uint8_t> encodings{
            {"Unspecified", PLDM_FRU_ENCODING_UNSPECIFIED},
            {"ASCII", PLDM_FRU_ENCODING_ASCII},
            {"UTF8", PLDM_FRU_ENCODING_UTF8},
            {"UTF16", PLDM_FRU_ENCODING_UTF16},
            {"UTF16LE", PLDM_FRU_ENCODING_UTF16LE},
            {"UTF16BE", PLDM_FRU_ENCODING_UTF16BE}};
        if (value.is_string())
        {
            auto it = encodings.find(value.get<std::string>());
            if (it == encodings.end())
            {
                throw std::invalid_argument("Unknown EncodingType");
            }
            return it->second;
        }
        return value.get<uint8_t>();
    }

    // Vendor IANA: uint32 little-endian; string: raw bytes; array: byte
    // sequence.
    static std::vector<uint8_t> encodeFieldValue(const nlohmann::json& value,
                                                 bool isVendorIana)
    {
        std::vector<uint8_t> bytes;
        if (isVendorIana)
        {
            if (!value.is_number_unsigned())
            {
                throw std::invalid_argument(
                    "Vendor IANA FieldValue must be an unsigned integer");
            }
            uint32_t iana = htole32(value.get<uint32_t>());
            bytes.resize(sizeof(iana));
            std::memcpy(bytes.data(), &iana, sizeof(iana));
        }
        else if (value.is_string())
        {
            const auto str = value.get<std::string>();
            bytes.assign(str.begin(), str.end());
        }
        else if (value.is_array())
        {
            for (const auto& element : value)
            {
                const auto byte = element.get<int>();
                if (byte < 0 || byte > 0xff)
                {
                    throw std::invalid_argument(
                        "FieldValue byte out of range 0-255");
                }
                bytes.push_back(static_cast<uint8_t>(byte));
            }
        }
        else
        {
            throw std::invalid_argument(
                "Unsupported FieldValue: expected string or array of bytes");
        }
        if (bytes.empty())
        {
            throw std::invalid_argument("FieldValue must not be empty");
        }
        if (bytes.size() > std::numeric_limits<uint8_t>::max())
        {
            throw std::invalid_argument("FieldValue length exceeds 255 bytes");
        }
        return bytes;
    }

    // Build the complete FRU record table blob from the JSON description:
    // one or more FRU records, followed by pad bytes to a 4-byte boundary and
    // a trailing CRC-32 integrity checksum (DSP0257 v1.0.1 Table 7).
    std::vector<uint8_t> buildFruRecordTable()
    {
        std::ifstream jsonFile(fruJsonPath);
        if (!jsonFile)
        {
            throw std::runtime_error("unable to open file");
        }
        auto data = nlohmann::json::parse(jsonFile);

        const auto& records = data.at("FRURecords");
        if (!records.is_array() || records.empty())
        {
            throw std::invalid_argument(
                "\"FRURecords\" must be a non-empty array");
        }

        constexpr size_t recordHeaderSize =
            sizeof(pldm_fru_record_data_format) - sizeof(pldm_fru_record_tlv);

        std::vector<uint8_t> table;
        for (const auto& record : records)
        {
            const uint16_t recordSetId =
                record.at("RecordSetIdentifier").get<uint16_t>();
            const uint8_t recordType = parseRecordType(record.at("RecordType"));
            const uint8_t encodingType =
                record.contains("EncodingType")
                    ? parseEncodingType(record.at("EncodingType"))
                    : static_cast<uint8_t>(PLDM_FRU_ENCODING_ASCII);

            // DSP0257 v1.0.1: Vendor IANA field type is 1 for OEM (Table 6),
            // 15 for General (Table 5, PLDM_FRU_FIELD_TYPE_IANA).
            constexpr uint8_t oemVendorIanaFieldType = 1;
            const uint8_t vendorIanaFieldType =
                (recordType == PLDM_FRU_RECORD_TYPE_OEM)
                    ? oemVendorIanaFieldType
                    : static_cast<uint8_t>(PLDM_FRU_FIELD_TYPE_IANA);

            std::vector<uint8_t> tlvs;
            uint8_t numFruFields = 0;
            bool hasIana = false;
            for (const auto& field : record.at("FieldEntries"))
            {
                const uint8_t fieldType = field.at("FieldType").get<uint8_t>();
                const bool isVendorIana = (fieldType == vendorIanaFieldType);
                auto fieldBytes =
                    encodeFieldValue(field.at("FieldValue"), isVendorIana);

                tlvs.emplace_back(fieldType);
                tlvs.emplace_back(static_cast<uint8_t>(fieldBytes.size()));
                tlvs.insert(tlvs.end(), fieldBytes.begin(), fieldBytes.end());
                numFruFields++;
                if (isVendorIana)
                {
                    hasIana = true;
                }
            }

            if (numFruFields == 0)
            {
                throw std::invalid_argument(
                    "each FRU record must contain at least one field");
            }
            // DSP0257 v1.0.1 Table 6: OEM records SHALL carry Vendor IANA;
            // warn (not fatal) to match libpldmresponder behavior.
            if (recordType == PLDM_FRU_RECORD_TYPE_OEM && !hasIana)
            {
                std::cerr
                    << "Warning: OEM FRU record (RecordSetIdentifier "
                    << recordSetId
                    << ") has no Vendor IANA field; DSP0257 v1.0.1 Table 6 "
                       "requires an OEM record to contain a field of type 1 "
                       "(Vendor IANA)"
                    << std::endl;
            }

            auto curSize = table.size();
            table.resize(curSize + recordHeaderSize + tlvs.size());
            auto rc = encode_fru_record(table.data(), table.size(), &curSize,
                                        recordSetId, recordType, numFruFields,
                                        encodingType, tlvs.data(), tlvs.size());
            if (rc != PLDM_SUCCESS)
            {
                throw std::runtime_error(
                    "encode_fru_record failed, rc=" + std::to_string(rc));
            }
        }

        if (table.empty())
        {
            throw std::invalid_argument("FRU record table is empty");
        }

        // DSP0257 v1.0.1 Table 7: pad the record data to a 4-byte boundary and
        // append the CRC-32 integrity checksum computed over the padded data.
        const uint8_t padBytes = pldm::utils::getNumPadBytes(table.size());
        table.insert(table.end(), padBytes, 0);

        uint32_t checksum =
            htole32(pldm_edac_crc32(table.data(), table.size()));
        const auto* checksumBytes = reinterpret_cast<const uint8_t*>(&checksum);
        table.insert(table.end(), checksumBytes,
                     checksumBytes + sizeof(checksum));

        return table;
    }

    std::string fruJsonPath;
    uint32_t maxTransferSize;
    std::vector<uint8_t> fruTable;
    size_t sentOffset;
    size_t curChunkLength;
    uint32_t dataTransferHandle;
    uint32_t partsSent;
    bool morePartsToSend;
};

void registerCommand(CLI::App& app)
{
    auto fru = app.add_subcommand("fru", "FRU type command");
    fru->require_subcommand(1);
    auto getFruRecordTableMetadata = fru->add_subcommand(
        "GetFruRecordTableMetadata", "get FRU record table metadata");
    commands.push_back(std::make_unique<GetFruRecordTableMetadata>(
        "fru", "GetFruRecordTableMetadata", getFruRecordTableMetadata));

    auto getFRURecordByOption =
        fru->add_subcommand("GetFRURecordByOption", "get FRU Record By Option");
    commands.push_back(std::make_unique<GetFRURecordByOption>(
        "fru", "GetFRURecordByOption", getFRURecordByOption));

    auto getFruRecordTable =
        fru->add_subcommand("GetFruRecordTable", "get FRU Record Table");
    commands.push_back(std::make_unique<GetFruRecordTable>(
        "fru", "GetFruRecordTable", getFruRecordTable));

    auto setFruRecordTable =
        fru->add_subcommand("SetFruRecordTable", "set FRU Record Table");
    commands.push_back(std::make_unique<SetFruRecordTable>(
        "fru", "SetFruRecordTable", setFruRecordTable));
}

} // namespace fru

} // namespace pldmtool
