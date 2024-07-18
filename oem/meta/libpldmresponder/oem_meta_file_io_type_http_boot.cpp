#include "oem_meta_file_io_type_http_boot.hpp"

#include <fcntl.h>
#include <sys/stat.h>

#include <phosphor-logging/lg2.hpp>

#include <fstream>
#include <utility>

PHOSPHOR_LOG2_USING;

namespace pldm::responder::oem_meta
{
uint8_t HTTP_BOOT_REQ_DATA_LEN = 4;

int HttpBootHandler::read(std::vector<uint8_t>& data)
{
    static constexpr auto certificationfilepath =
        "/mnt/data/host/bios-rootcert";

    if (data.size() != HTTP_BOOT_REQ_DATA_LEN)
    {
        error("{FUNC}: Invalid incoming data for http boot, data size={SIZE}",
              "FUNC", std::string(__func__), "SIZE", data.size());
        return PLDM_ERROR;
    }

    uint8_t length = data.at(0);
    uint8_t trasferFlag = data.at(1);
    uint8_t highOffset = data.at(2);
    uint8_t lowOffset = data.at(3);
    uint16_t offset = (static_cast<uint16_t>(highOffset) << 8) |
                      static_cast<uint16_t>(lowOffset);

    if (access(certificationfilepath, F_OK) == -1)
    {
        error("Failed to find Http boot certification file");
        return PLDM_ERROR;
    }

    int fd = open(certificationfilepath, O_RDONLY);
    if (fd < 0)
    {
        error("Failed to open Http boot certification file");
        return PLDM_ERROR;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        error("Failed to get Http boot certification file size");
        return PLDM_ERROR;
    }

    if (sb.st_size == 0)
    {
        data.at(0) = sb.st_size;
        data.at(1) = PLDM_END;
        data.at(2) = 0;
        data.at(3) = 0;
        return PLDM_SUCCESS;
    }

    int ret = lseek(fd, offset, SEEK_SET);
    if (ret < 0)
    {
        error(
            "Failed to lseek at offset={OFFSET} of length={LENGTH} on Http boot certification file",
            "OFFSET", offset, "LENGTH", length);
        return PLDM_ERROR;
    }

    if (offset + length >= sb.st_size)
    {
        trasferFlag = PLDM_END;
        length = sb.st_size - offset; // Revise length
    }
    else
    {
        trasferFlag = PLDM_MIDDLE;
    }

    uint8_t* buffer = (uint8_t*)malloc(length);
    if (buffer == NULL)
    {
        error(
            "{FUNC}: Failed to allocate buffer for http boot, length={LENGTH}",
            "FUNC", std::string(__func__), "LENGTH", length);
        return PLDM_ERROR;
    }

    ret = ::read(fd, buffer, length);
    if (ret < 0)
    {
        error(
            "Failed to read file content at offset={OFFSET} of length={LENGTH} on Http boot certification file",
            "OFFSET", offset, "LENGTH", length);
        free(buffer);
        return PLDM_ERROR;
    }

    data.insert(data.end(), buffer, buffer + length);
    free(buffer);

    offset = offset + length;
    highOffset = (offset >> 8) & 0xFF;
    lowOffset = offset & 0xFF;
    data.at(0) = length;
    data.at(1) = trasferFlag;
    data.at(2) = highOffset;
    data.at(3) = lowOffset;

    return PLDM_SUCCESS;
}

} // namespace pldm::responder::oem_meta
