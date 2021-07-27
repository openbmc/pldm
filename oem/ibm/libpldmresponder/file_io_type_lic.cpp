#include "file_io_type_lic.hpp"

#include "libpldm/base.h"
#include "oem/ibm/libpldm/file_io.h"

#include "common/utils.hpp"

#include <stdint.h>

#include <iostream>

namespace pldm
{
namespace responder
{

static constexpr auto codFilePath = "/var/lib/ibm/cod/";

int LicenseHandler::readIntoMemory(
    uint32_t offset, uint32_t& length, uint64_t address,
    oem_platform::Handler* /*oemPlatformHandler*/)
{
    if (licType != PLDM_FILE_TYPE_LICENSE_COD)
    {
        return PLDM_ERROR_INVALID_DATA;
    }
    auto rc = transferFileData(filePath.c_str(), true, offset, length, address);
    if (rc)
    {
        return PLDM_ERROR;
    }
    return PLDM_SUCCESS;
}

int LicenseHandler::read(uint32_t offset, uint32_t& length, Response& response,
                         oem_platform::Handler* /*oemPlatformHandler*/)
{
    if (licType != PLDM_FILE_TYPE_LICENSE_COD)
    {
        return PLDM_ERROR_INVALID_DATA;
    }
    auto rc = readFile(filePath.c_str(), offset, length, response);
    if (rc)
    {
        return PLDM_ERROR;
    }
    return PLDM_SUCCESS;
}

int LicenseHandler::newFileAvailable(uint64_t length)
{
    fs::create_directories(codFilePath);
    fs::permissions(codFilePath,
                    fs::perms::others_read | fs::perms::owner_write);
    int fileFd = -1;
    int flags = O_WRONLY | O_CREAT | O_TRUNC;
    std::string filePath = certFilePath;

    if (licType == PLDM_FILE_TYPE_LICENSE_COD)
    {
        fileFd = open(
            (filePath + "Cod_license_" + std::to_string(fileHandle)).c_str(),
            flags, S_IRUSR | S_IWUSR);
    }
    if (fileFd == -1)
    {
        std::cerr << "failed to open file for type " << certType
                  << " ERROR=" << errno << "\n";
        return PLDM_ERROR;
    }
    return PLDM_SUCCESS;
}

} // namespace responder
} // namespace pldm
