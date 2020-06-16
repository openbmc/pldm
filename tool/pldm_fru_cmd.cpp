#include "pldm_fru_cmd.hpp"

#include "pldm_cmd_helper.hpp"

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
    ~GetFruRecordTableMetadata() = default;
    GetFruRecordTableMetadata() = delete;
    GetFruRecordTableMetadata(const GetFruRecordTableMetadata&) = delete;
    GetFruRecordTableMetadata(GetFruRecordTableMetadata&&) = default;
    GetFruRecordTableMetadata&
        operator=(const GetFruRecordTableMetadata&) = delete;
    GetFruRecordTableMetadata& operator=(GetFruRecordTableMetadata&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr));
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

        auto rc = encode_get_fru_record_table_metadata_req(
            instanceId, request, PLDM_GET_FRU_RECORD_TABLE_METADATA_REQ_BYTES);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;
        uint8_t fru_data_major_version, fru_data_minor_version;
        uint32_t fru_table_maximum_size, fru_table_length;
        uint16_t total_record_set_identifiers, total_table_records;
        uint32_t checksum;

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
        std::cout << "FRUDATAMajorVersion : "
                  << static_cast<uint32_t>(fru_data_major_version) << std::endl;
        std::cout << "FRUDATAMinorVersion : "
                  << static_cast<uint32_t>(fru_data_minor_version) << std::endl;
        std::cout << "FRUTableMaximumSize : " << fru_table_maximum_size
                  << std::endl;
        std::cout << "FRUTableLength : " << fru_table_length << std::endl;
        std::cout << "Total number of Record Set Identifiers in table : "
                  << total_record_set_identifiers << std::endl;
        std::cout << "Total number of records in table :  "
                  << total_table_records << std::endl;
        std::cout << "FRU DATAStructureTableIntegrityChecksum :  " << checksum
                  << std::endl;
    }
};

class FRUTablePrint
{
  public:
    explicit FRUTablePrint(const uint8_t* table, size_t table_size) :
        table(table), table_size(table_size)
    {
    }

    void print()
    {
        auto p = table;
        while (!isTableEnd(p))
        {
            auto record =
                reinterpret_cast<const pldm_fru_record_data_format*>(p);
            std::cout << "FRU Record Set Identifier: "
                      << (int)le16toh(record->record_set_id) << std::endl;
            std::cout << "FRU Record Type: "
                      << typeToString(fruRecordTypes, record->record_type)
                      << std::endl;
            std::cout << "Number of FRU fields: " << (int)record->num_fru_fields
                      << std::endl;
            std::cout << "Encoding Type for FRU fields: "
                      << typeToString(fruEncodingType, record->encoding_type)
                      << std::endl;

            if (record->record_type != PLDM_FRU_RECORD_TYPE_GENERAL)
            {
                throw std::runtime_error("Unspported Record Type");
            }

            p += sizeof(pldm_fru_record_data_format) -
                 sizeof(pldm_fru_record_tlv);
            for (int i = 0; i < record->num_fru_fields; i++)
            {
                auto tlv = reinterpret_cast<const pldm_fru_record_tlv*>(p);
                std::cout << "\tFRU Field Type: "
                          << typeToString(fruTableFieldTypes, tlv->type)
                          << std::endl;
                std::cout << "\tFRU Field Length: " << (int)(tlv->length)
                          << std::endl;
                if (tlv->type > 15)
                {
                    throw std::runtime_error("Unspported Field Type");
                }

                if (tlv->type == PLDM_FRU_FIELD_TYPE_MANUFAC_DATE)
                {
                    std::cout << "\tFRU Field Value: TODO" << std::endl;
                }
                else
                {

                    std::cout << "\tFRU Field Value: "
                              << fruFieldValuestring(tlv->value, tlv->length)
                              << std::endl;
                }
                p += sizeof(pldm_fru_record_tlv) - 1 + tlv->length;
            }
        }
    }

  private:
    const uint8_t* table;
    size_t table_size;

    bool isTableEnd(const uint8_t* p)
    {
        auto offset = p - table;
        return (table_size - offset) <= 7;
    }

    static inline const std::map<uint8_t, const char*> fruEncodingType{
        {PLDM_FRU_ENCODING_UNSPECIFIED, "Unspecified"},
        {PLDM_FRU_ENCODING_ASCII, "ASCII"},
        {PLDM_FRU_ENCODING_UTF8, "UTF8"},
        {PLDM_FRU_ENCODING_UTF16, "UTF16"},
        {PLDM_FRU_ENCODING_UTF16LE, "UTF16LE"},
        {PLDM_FRU_ENCODING_UTF16BE, "UTF16BE"}};

    static inline const std::map<uint8_t, const char*> fruTableFieldTypes{
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

    static inline const std::map<uint8_t, const char*> fruRecordTypes{
        {PLDM_FRU_RECORD_TYPE_GENERAL, "General"},
        {PLDM_FRU_RECORD_TYPE_OEM, "OEM"}};

    std::string typeToString(std::map<uint8_t, const char*> typeMap,
                             uint8_t type)
    {
        auto typeString = std::to_string(type);
        try
        {
            return typeString + "(" + typeMap.at(type) + ")";
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
};

class GetFRURecordByOption : public CommandInterface
{
  public:
    ~GetFRURecordByOption() = default;
    GetFRURecordByOption() = delete;
    GetFRURecordByOption(const GetFRURecordByOption&) = delete;
    GetFRURecordByOption(GetFruRecordTableMetadata&&) = delete;
    GetFRURecordByOption& operator=(const GetFRURecordByOption&) = delete;
    GetFRURecordByOption& operator=(GetFRURecordByOption&&) = delete;

    explicit GetFRURecordByOption(const char* type, const char* name,
                                  CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-i, --identifier", recordSetIdentifier,
                        "Record Set Identifier\n"
                        "Possible values: {All record sets=0x0000, Specific "
                        "record set=0x0001 – 0xffff}")
            ->required();
        app->add_option("-r, --record", recordType,
                        "Record Type\n"
                        "Possible values: {All record types=0x00, Specific "
                        "record types=0x01 – 0xff}")
            ->required();
        app->add_option("-f, --field", fieldType,
                        "Field Type\n"
                        "Possible values: {All record field types=0x00, "
                        "Specific field types=0x01 – 0xff}")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        if (fieldType != 0 && recordType == 0)
        {
            throw std::invalid_argument("if filed type is non-zero, the record "
                                        "type shall also be non-zero");
        }

        auto payloadLength = sizeof(pldm_get_fru_record_by_option_req);

        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) + payloadLength,
                                        0);
        auto reqMsg = reinterpret_cast<pldm_msg*>(requestMsg.data());

        auto rc = encode_get_fru_record_by_option_req(
            instanceId, 0 /* DataTransferHandle */, 0 /* FRUTableHandle */,
            recordSetIdentifier, recordType, fieldType, PLDM_GET_FIRSTPART,
            reqMsg, payloadLength);

        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc;
        uint32_t dataTransferHandle;
        uint8_t transferFlag;
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
}

} // namespace fru

} // namespace pldmtool
