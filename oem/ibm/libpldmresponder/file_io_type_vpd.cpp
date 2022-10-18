#include "file_io_type_vpd.hpp"

#include "libpldm/base.h"
#include "oem/ibm/libpldm/file_io.h"

#include "common/utils.hpp"

#include <stdint.h>

#include <iostream>

namespace pldm
{
using namespace pldm::responder::utils;
using namespace pldm::utils;

namespace responder
{

static constexpr auto keyWrdFilePath = "/var/lib/ibm/vpd/";

int keywordHandler::read(uint32_t offset, uint32_t& length, Response& response,
                         oem_platform::Handler* /*oemPlatformHandler*/)
{
    std::string filePath = keyWrdFilePath;
    filePath += "keyWrdFile";

    if (vpdFileType != PLDM_FILE_TYPE_VPD_KEYWORD)
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
