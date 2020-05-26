#include "pldm_file_cmd.hpp"

#include "../../pldm_cmd_helper.hpp"

#include <iostream>
#include <string>

#include "oem/ibm/libpldm/file_io.h"
#include "pldm_types.h"

namespace pldmtool
{

namespace oem_ibm
{
namespace file_io
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
        size_t fileTableDataLength = 0;
        auto responsePtr =
            reinterpret_cast<struct pldm_msg*>(responseMsg.data());
        auto payloadLength = responseMsg.size() - sizeof(pldm_msg_hdr);

        std::vector<uint8_t> fileTableData(payloadLength);
        rc = decode_get_file_table_resp(
            responsePtr, payloadLength, &cc, &nextTransferHandle, &transferFlag,
            fileTableData.data(), &fileTableDataLength);

        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << ", rc=" << rc << ", cc=" << (int)cc << std::endl;
            return;
        }

        printFileAttrTable(fileTableData.data(), fileTableDataLength);
    }

    void printFileAttrTable(uint8_t* data, size_t length)
    {
        if (data == NULL || length == 0)
        {
            return;
        }
        pldm_file_attr_table_entry* filetableData =
            reinterpret_cast<pldm_file_attr_table_entry*>(data);

        uint8_t* fileData =
            reinterpret_cast<uint8_t*>(filetableData->file_attr_table_nst);

        for (size_t i = 0; i < sizeof(data); ++i)
        {
            if (length <= 8)
            {
                break;
            }

            std::cout << "FileHandle[" << i
                      << "]:" << filetableData->file_handle << std::endl;
            std::cout << "  FileNameLength[" << i
                      << "]:" << filetableData->file_name_length << std::endl;

            int nameLength = filetableData->file_name_length;
            char name[256];
            memcpy(&name, reinterpret_cast<uint8_t*>(fileData), nameLength);

            std::cout << "  FileName[" << i << "]:" << fileData << std::endl;
            fileData += nameLength;

            int size;
            memcpy(&size, reinterpret_cast<uint8_t*>(fileData),
                   sizeof(uint32_t));
            std::cout << "  FileSize[" << i << "]:" << size << std::endl;
            fileData += sizeof(uint32_t);

            bitfield32_t bf;
            memcpy(&bf.value, reinterpret_cast<uint8_t*>(fileData),
                   sizeof(bitfield32_t));
            std::cout << "  FileTraits[" << i << "]:" << (bf.value)
                      << std::endl;

            fileData += sizeof(bitfield32_t);

            // Incrementing the pointer to point to the next file table
            // record

            filetableData =
                reinterpret_cast<pldm_file_attr_table_entry*>(fileData);
            fileData =
                reinterpret_cast<uint8_t*>(filetableData->file_attr_table_nst);

            length -= sizeof(uint32_t) + sizeof(uint16_t) + nameLength +
                      sizeof(uint32_t) + sizeof(bitfield32_t);
        }
    }

  private:
    pldm_fileio_table_type pldmFileioTableType;
};

void registerCommand(CLI::App& app)
{

    auto oem_ibm = app.add_subcommand("oem-ibm-fileIO", "oem type command");
    oem_ibm->require_subcommand(1);

    auto getFileTable =
        oem_ibm->add_subcommand("GetFileTable", "get file table");

    commands.push_back(std::make_unique<GetFileTable>(
        "oem_ibm_fileIO", "getFileTable", getFileTable));
}

} // namespace file_io

} // namespace oem_ibm
} // namespace pldmtool
