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
    GetFruRecordTable&
        operator=(const GetFruRecordTable&) = delete;
    GetFruRecordTable& operator=(GetFruRecordTable&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

        auto rc = encode_get_fru_record_table_req(instanceId, 0, PLDM_START_AND_END, request, requestMsg.size() - sizeof(pldm_msg_hdr));
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;
        uint32_t next_data_transfer_handle = 0;
        uint8_t transfer_flag = 0;
        uint8_t fru_record_table_data;
        size_t fru_record_table_length = 0;

        auto rc = decode_get_fru_record_table_resp(
            responsePtr, payloadLength, &cc, &next_data_transfer_handle,
            &transfer_flag, &fru_record_table_data, &fru_record_table_length);

        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << std::endl;
            return;
        }
        std::cout << "payloadLength: " << payloadLength << std::endl;
        std::cout << "fru_record_table_data: " << fru_record_table_data  << std::endl;
        std::cout << "FRURecordTablelength: "
                  << fru_record_table_length << std::endl;
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

    auto getFruRecordTable = fru->add_subcommand(
        "GetFruRecordTable", "get FRU record table");
    commands.push_back(std::make_unique<GetFruRecordTable>(
        "fru", "GetFruRecordTable", getFruRecordTable));

}

} // namespace fru

} // namespace pldmtool
