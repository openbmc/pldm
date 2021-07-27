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
    std::string filePath = codFilePath;
    filePath += "licFile"

        if (licType != PLDM_FILE_TYPE_LICENSE_COD)
    {
        return PLDM_ERROR_INVALID_DATA;
    }
    auto rc = transferFileData(filePath.c_str(), true, offset, length, address);
    fs::remove(filePath);
    if (rc)
    {
        return PLDM_ERROR;
    }
    return PLDM_SUCCESS;
}

int LicenseHandler::read(uint32_t offset, uint32_t& length, Response& response,
                         oem_platform::Handler* /*oemPlatformHandler*/)
{
    std::string filePath = codFilePath;
    filePath += "licFile"

        if (licType != PLDM_FILE_TYPE_LICENSE_COD)
    {
        return PLDM_ERROR_INVALID_DATA;
    }
    auto rc = readFile(filePath.c_str(), offset, length, response);
    fs::remove(filePath);
    if (rc)
    {
        return PLDM_ERROR;
    }
    return PLDM_SUCCESS;
}

} // namespace responder
} // namespace pldm
