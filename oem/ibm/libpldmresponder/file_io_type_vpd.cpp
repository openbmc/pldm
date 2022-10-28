#include "file_io_type_vpd.hpp"

#include "libpldm/base.h"
#include "libpldm/file_io.h"

#include "common/utils.hpp"

#include <stdint.h>

#include <iostream>

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

    namespace fs = std::filesystem;
    std::string dirPath = "/tmp/vpd";
    std::string filePath = dirPath + "/vpdKeywrd";
    const fs::path keywrdDirPath = dirPath;
    fs::path keywrdFilePath = keywrdDirPath / "vpdKeywrd";

    static constexpr auto keywrdObjPath =
        "xyz/openbmc_project/inventory/system/chassis/motherboard/vdd_vrm0";
    static constexpr auto keywrdPropName = "PD_D";
    static constexpr auto keywrdInterface = "com.ibm.ipzvpd.VPRI";
    std::string keywrdStr;

    try
    {
        keywrdStr = pldm::utils::DBusHandler().getDbusProperty<std::string>(
            keywrdObjPath, keywrdPropName, keywrdInterface);
    }
    catch (const std::exception& e)
    {
        return PLDM_ERROR;
    }

    if (!fs::exists(keywrdDirPath))
    {
        fs::create_directories(keywrdDirPath);
        fs::permissions(keywrdDirPath,
                        fs::perms::others_read | fs::perms::owner_write);
    }

    std::ofstream keywrdFile;

    keywrdFile.open(keywrdFilePath, std::ios::out | std::ofstream::binary);

    if (!keywrdFile)
    {
        std::cerr << "VPD keyword file open error: " << keywrdFilePath << "\n";
        return PLDM_ERROR;
    }

    // Add keyword string to file
    keywrdFile << keywrdStr << std::endl;
    keywrdFile.close();

    uint32_t fileSize = fs::file_size(keywrdFilePath);

    auto rc = readFile(filePath.c_str(), offset, fileSize, response);

    if (rc)
    {
        return PLDM_ERROR;
    }
    return PLDM_SUCCESS;
}

} // namespace responder
} // namespace pldm
