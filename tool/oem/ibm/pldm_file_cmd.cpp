#include "pldm_file_cmd.hpp"

#include "../../pldm_cmd_helper.hpp"

#include <iostream>
#include <string>

#include "oem/ibm/libpldm/file_io.h"
#include "pldm_types.h"

namespace pldmtool
{

namespace oem_ibm_fileIO
{
namespace file
{
using namespace pldmtool::helper;

std::vector<std::unique_ptr<CommandInterface>> commands;

const std::map<const char*, pldm_fileio_table_type> pldmFileIOTableTypes{
    {"AttributeTable", PLDM_FILE_ATTRIBUTE_TABLE},
    {"OEMAttributeTable", PLDM_OEM_FILE_ATTRIBUTE_TABLE},
};

class GetFileTable : public CommandInterface
{
  public:
    ~GetFileTable() = default;
    GetFileTable() = delete;
    GetFileTable(const GetFileTable&) = delete;
    GetFileTable(GetFileTable&&) = default;
    GetFileTable& operator=(const GetFileTable&) = delete;
    GetFileTable& operator=(GetFileTable&&) = default;

    explicit GetFileTable(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-t,--type", pldmFileioTableType,
                        "pldm file table type")
            ->required()
            ->transform(CLI::CheckedTransformer(pldmFileIOTableTypes,
                                                CLI::ignore_case));
    }
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {

        return {PLDM_ERROR, {}};
    }

    void parseResponseMsg(pldm_msg*, size_t) override
    {
    }
    void exec()
    {

        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                        PLDM_GET_FILE_TABLE_REQ_BYTES);

        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

        auto rc = encode_get_file_table_req(instanceId, 0, PLDM_GET_FIRSTPART,
                                            pldmFileioTableType, request);
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
        std::vector<uint8_t> file_table_data(16777216);
        size_t file_table_data_length = 0;
        auto responsePtr =
            reinterpret_cast<struct pldm_msg*>(responseMsg.data());
        auto payloadLength = responseMsg.size() - sizeof(pldm_msg_hdr);

        rc = decode_get_file_table_resp(
            responsePtr, payloadLength, &cc, &nextTransferHandle, &transferFlag,
            file_table_data.data(), &file_table_data_length);

        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << ", rc=" << rc << ", cc=" << (int)cc << std::endl;
            return;
        }

        printFileTable(file_table_data.data(), file_table_data_length);
    }

    void printFileAttrTable(uint8_t* data, size_t length)
    {
        if (data == NULL || length == 0)
        {
            return;
        }
        pldm_file_attr_table_entry* fileData =
            reinterpret_cast<pldm_file_attr_table_entry*>(data);

        uint8_t* file_data = reinterpret_cast<uint8_t*>(fileData->rest_params);

        for (size_t i = 0; i < sizeof(data); ++i)
        {
            if (length <= 8)
            {
                break;
            }
            else
            {
                std::cout << "FileHandle[" << i << "]:" << fileData->file_handle
                          << std::endl;
                std::cout << "FileNameLength[" << i
                          << "]:" << fileData->file_name_length << std::endl;

                int name_length = fileData->file_name_length;
                char name[256];
                memmove(&name, reinterpret_cast<uint8_t*>(file_data),
                        name_length);

                std::cout << "FileName[" << i << "]:" << file_data << std::endl;
                file_data += name_length;

                int size;
                memcpy(&size, reinterpret_cast<uint8_t*>(file_data),
                       sizeof(uint32_t));
                std::cout << "FileSize[" << i << "]:" << size << std::endl;
                file_data += sizeof(uint32_t);

                bitfield32_t bf;
                memcpy(&bf.value, reinterpret_cast<uint8_t*>(file_data),
                       sizeof(bitfield32_t));
                std::cout << "FileTraits[" << i << "]:" << (bf.value)
                          << std::endl;

                file_data += sizeof(bitfield32_t);
                int ar = name_length + sizeof(uint32_t) + sizeof(bitfield32_t);
                uint8_t* temp = reinterpret_cast<uint8_t*>(fileData);
                temp += sizeof(uint32_t) + sizeof(uint16_t) + ar;
                fileData = reinterpret_cast<pldm_file_attr_table_entry*>(temp);
                file_data = reinterpret_cast<uint8_t*>(fileData->rest_params);
                length -= sizeof(uint32_t) + sizeof(uint16_t) + ar;
            }
        }
    }

    void printFileTable(uint8_t* data, size_t length)
    {
        if (data == NULL || length == 0)
        {
            return;
        }

        printFileAttrTable(data, length);
    }

  private:
    pldm_fileio_table_type pldmFileioTableType;
};

void registerCommand(CLI::App& app)
{

    auto oem_ibm_fileIO =
        app.add_subcommand("oem-ibm-fileIO", "oem type command");
    oem_ibm_fileIO->require_subcommand(1);

    auto getFileTable =
        oem_ibm_fileIO->add_subcommand("GetFileTable", "get file table");

    commands.push_back(std::make_unique<GetFileTable>(
        "oem_ibm_fileIO", "getFileTable", getFileTable));
}

} // namespace file

} // namespace oem_ibm_fileIO
} // namespace pldmtool
