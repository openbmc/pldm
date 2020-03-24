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

        auto rc = encode_get_fru_record_table_metadata_req(instanceId, request);
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

class GetFruRecordTable : public CommandInterface
{
  public:
    ~GetFruRecordTable() = default;
    GetFruRecordTable() = delete;
    GetFruRecordTable(const GetFruRecordTable&) = delete;
    GetFruRecordTable(GetFruRecordTable&&) = default;
    GetFruRecordTable& operator=(const GetFruRecordTable&) = delete;
    GetFruRecordTable& operator=(GetFruRecordTable&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                        PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

        auto rc = encode_get_fru_record_table_req(
            instanceId, 0, PLDM_START_AND_END, request,
            requestMsg.size() - sizeof(pldm_msg_hdr));
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;
        uint32_t next_data_transfer_handle = 0;
        uint8_t transfer_flag = 0;
        std::vector<uint8_t> fru_record_table_data(16777216);
        size_t fru_record_table_length = 0;

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

        printFRUTable(fru_record_table_data.data(), fru_record_table_length);
    }

  private:
    const std::map<uint8_t, const char*> FruEncodingTypes{
        {PLDM_FRU_ENCODING_UNSPECIFIED, "Unspecified"},
        {PLDM_FRU_ENCODING_ASCII, "ASCII"},
        {PLDM_FRU_ENCODING_UTF8, "UTF8"},
        {PLDM_FRU_ENCODING_UTF16, "UTF16"},
        {PLDM_FRU_ENCODING_UTF16LE, "UTF16LE"},
        {PLDM_FRU_ENCODING_UTF16BE, "UTF16BE"}};

    const std::map<uint8_t, const char*> FruTableFieldTypes{
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

    const std::map<uint8_t, const char*> FruRecordTypes{
        {PLDM_FRU_RECORD_TYPE_GENERAL, "General"},
        {PLDM_FRU_RECORD_TYPE_OEM, "OEM"}};

    std::string getFieldType(uint8_t type)
    {
        try
        {
            return FruTableFieldTypes.at(type);
        }
        catch (const std::out_of_range& e)
        {
            return std::to_string(static_cast<unsigned>(type));
        }
    }

    std::string FruFieldValuestring(uint8_t* value, uint8_t length)
    {
        return std::string(reinterpret_cast<const char*>(value), length);
    }
    void printFRUTable(uint8_t* data, size_t len)
    {
        if (data == NULL || len == 0)
        {
            return;
        }
        struct pldm_fru_record_data_format* fru =
            (struct pldm_fru_record_data_format*)data;

        std::cout << "FRU Record Set Identifier: " << fru->record_set_id
                  << std::endl;
        std::cout << "FRU Record Type: " << FruRecordTypes.at(fru->record_type)
                  << std::endl;
        std::cout << "Number of FRU fields: " << unsigned(fru->num_fru_fields)
                  << std::endl;
        std::cout << "Encoding Type for FRU fields: "
                  << FruEncodingTypes.at(fru->encoding_type) << std::endl
                  << std::endl;
        std::cout << "FRU Table Data: " << std::endl;
        for (size_t i = 0; i < fru->num_fru_fields; ++i)
        {
            struct pldm_fru_record_tlv* frutable =
                (struct pldm_fru_record_tlv*)fru->tlvs +
                i * sizeof(pldm_fru_record_tlv);

            if (frutable->type < 16)
            {
                std::cout << "FRU Field Type: " << getFieldType(frutable->type)
                          << std::endl;
                std::cout << "FRU Field Length: " << unsigned(frutable->length)
                          << std::endl;
                std::cout << "FRU Field Value: "
                          << FruFieldValuestring((frutable->value),
                                                 unsigned(frutable->length))
                          << std::endl;
            }
        }
    }
};

void registerCommand(CLI::App& app)
{
    auto fru = app.add_subcommand("fru", "FRU type command");
    fru->require_subcommand(1);
    auto getFruRecordTableMetadata = fru->add_subcommand(
        "GetFruRecordTableMetadata", "get FRU record table metadata");
    commands.push_back(std::make_unique<GetFruRecordTableMetadata>(
        "fru", "GetFruRecordTableMetadata", getFruRecordTableMetadata));

    auto getFruRecordTable =
        fru->add_subcommand("GetFruRecordTable", "get FRU record table");
    commands.push_back(std::make_unique<GetFruRecordTable>(
        "fru", "GetFruRecordTable", getFruRecordTable));
}

} // namespace fru

} // namespace pldmtool
