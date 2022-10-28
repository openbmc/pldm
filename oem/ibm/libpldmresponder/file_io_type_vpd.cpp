#include "file_io_type_vpd.hpp"

#include "libpldm/base.h"
#include "libpldm/file_io.h"

#include "common/utils.hpp"

#include <stdint.h>

#include <iostream>

typedef unsigned char byte;

namespace pldm
{
namespace responder
{
int keywordHandler::read(uint32_t offset, uint32_t& /*length*/,
                         Response& response,
                         oem_platform::Handler* /*oemPlatformHandler*/)
{
    if (vpdFileType != PLDM_FILE_TYPE_VPD_KEYWORD)
    {
        return PLDM_ERROR_INVALID_DATA;
    }

    const char* keywrdObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/vdd_vrm0";
    const char* keywrdPropName = "PD_D";
    const char* keywrdInterface = "com.ibm.ipzvpd.VPRI";

    std::cerr << "VPD #D : inside read "
              << "\n";

    std::variant<std::vector<byte>> keywrd;

    auto& bus = DBusHandler::getBus();
    auto service =
        pldm::utils::DBusHandler().getService(keywrdObjPath, keywrdInterface);
    auto method = bus.new_method_call(service.c_str(), keywrdObjPath,
                                      "org.freedesktop.DBus.Properties", "Get");
    method.append(keywrdInterface, keywrdPropName);
    auto reply = bus.call(method);
    reply.read(keywrd);

    auto keywrdSize = std::get<std::vector<byte>>(keywrd).size();
    std::cerr << "VPD keyword size : " << keywrdSize << std::endl;

    namespace fs = std::filesystem;
    std::string dirPath = "/tmp/vpd";
    std::string filePath = dirPath + "/vpdKeywrd.bin";
    const fs::path keywrdDirPath = dirPath;
    fs::path keywrdFilePath = keywrdDirPath / "vpdKeywrd.bin";

    if (!fs::exists(keywrdDirPath))
    {
        fs::create_directories(keywrdDirPath);
        fs::permissions(keywrdDirPath,
                        fs::perms::others_read | fs::perms::owner_write);
    }

    std::ofstream keywrdFile("vpdKeywrd.bin");
    keywrdFile.open(keywrdFilePath, std::ios::out | std::ofstream::binary);
    if (!keywrdFile)
    {
        std::cerr << "VPD keyword file open error: " << keywrdFilePath << "\n";
        return PLDM_ERROR;
    }
    keywrdFile.write((const char*)&(std::get<std::vector<byte>>(keywrd)[0]),
                     keywrdSize);
    keywrdFile.close();

    auto rc = readFile(filePath.c_str(), offset, keywrdSize, response);
    if (rc)
    {
        return PLDM_ERROR;
    }
    return PLDM_SUCCESS;
}
} // namespace responder
} // namespace pldm
