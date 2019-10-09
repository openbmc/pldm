#include "config.h"

#include "lid_file_io.hpp"

#include "libpldmresponder/utils.hpp"

#include <stdint.h>
#include <unistd.h>

#include <filesystem>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>

#include "libpldm/base.h"
#include "oem/ibm/libpldm/file_io.h"

namespace pldm
{
using namespace phosphor::logging;

namespace responder
{

namespace oem_file_type
{

int LidHandler::readIntoMemory()
{
    if (!fs::exists(path))
    {
        log<level::ERR>("File does not exist", entry("PATH=%s", path.c_str()));
        return PLDM_INVALID_FILE_HANDLE;
    }

    size_t fileSize = fs::file_size(path);
    auto tmpLength = length;

    if (offset >= fileSize)
    {
        log<level::ERR>("Offset exceeds file size", entry("OFFSET=%d", offset),
                        entry("FILE_SIZE=%d", fileSize));
        return PLDM_DATA_OUT_OF_RANGE;
    }

    if (offset + tmpLength > fileSize)
    {
        tmpLength = fileSize - offset;
    }

    if (tmpLength % dma::minSize)
    {
        log<level::ERR>("Read length is not a multiple of DMA minSize",
                        entry("LENGTH=%d", tmpLength));
        return PLDM_INVALID_READ_LENGTH;
    }

    length = tmpLength;
    auto rc = this->transferFileData(path, true);
    return rc;
}

fs::path createLidPath(uint32_t fileHandle)

{
    std::stringstream stream;
    stream << std::hex << fileHandle;
    std::string lidName(stream.str());
    lidName += ".lid";
    char sep = '/';
    std::string lidPath(LID_PRIM_DIR);
    lidPath += sep + lidName;
    fs::path path(lidPath);
    return path;
}

} // namespace oem_file_type

} // namespace responder

} // namespace pldm
