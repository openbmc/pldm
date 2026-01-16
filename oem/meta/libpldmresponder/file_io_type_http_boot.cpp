#include "file_io_type_http_boot.hpp"

#include <fcntl.h>
#include <libpldm/edac.h>
#include <sys/stat.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <cstring>
#include <fstream>
#include <utility>

PHOSPHOR_LOG2_USING;

namespace pldm::responder::oem_meta
{

constexpr uint8_t HTTP_BOOT_ATTR_REQ_DATA_LEN = 0;

static constexpr auto certificationfilepath = "/mnt/data/host/bios-rootcert";

int HttpBootHandler::read(struct pldm_oem_meta_file_io_read_resp* data)
{
    if (data == nullptr)
    {
        error("Input data pointer is NULL");
        return PLDM_ERROR;
    }

    int fd = -1;
    struct stat sb;

    if (access(certificationfilepath, F_OK) == -1)
    {
        error("Failed to find Http boot certification file");
        goto noCertificationFile;
    }

    fd = open(certificationfilepath, O_RDONLY);
    if (fd < 0)
    {
        error("Failed to open Http boot certification file");
        goto noCertificationFile;
    }

    if (fstat(fd, &sb) == -1)
    {
        error("Failed to get Http boot certification file size");
        goto noCertificationFile;
    }

    switch (data->option)
    {
        case PLDM_OEM_META_FILE_IO_READ_ATTR:
        {
            if (data->length != HTTP_BOOT_ATTR_REQ_DATA_LEN)
            {
                error(
                    "Invalid request data length for http boot attribute, option={OPTION}, data size={SIZE}",
                    "OPTION", data->option, "SIZE", data->length);
                close(fd);
                return PLDM_ERROR;
            }

            if (sb.st_size == 0)
            {
                data->info.attr.size = 0;
                data->info.attr.crc32 = 0;
                close(fd);
                return PLDM_SUCCESS;
            }

            uint8_t* buffer = (uint8_t*)malloc(sb.st_size);
            if (buffer == nullptr)
            {
                error(
                    "Failed to allocate buffer for http boot, length={LENGTH}",
                    "LENGTH", sb.st_size);
                close(fd);
                return PLDM_ERROR;
            }

            int ret = ::read(fd, buffer, sb.st_size);
            if (ret < 0)
            {
                error("Failed to read all Http boot certification file");
                free(buffer);
                close(fd);
                return PLDM_ERROR;
            }

            uint32_t checksum = pldm_edac_crc32(buffer, sb.st_size);
            free(buffer);

            data->info.attr.size = sb.st_size;
            data->info.attr.crc32 = checksum;
            close(fd);
            return PLDM_SUCCESS;
        }
        break;
        case PLDM_OEM_META_FILE_IO_READ_DATA:
        {
            if (sb.st_size == 0)
            {
                data->length = 0;
                data->info.data.transferFlag = PLDM_END;
                data->info.data.offset = 0;
                close(fd);
                return PLDM_SUCCESS;
            }

            uint8_t transferFlag = data->info.data.transferFlag;
            uint16_t offset = data->info.data.offset;

            int ret = lseek(fd, offset, SEEK_SET);
            if (ret < 0)
            {
                error(
                    "Failed to lseek at offset={OFFSET} of length={LENGTH} on Http boot certification file",
                    "OFFSET", offset, "LENGTH", data->length);
                close(fd);
                return PLDM_ERROR;
            }

            if (offset + data->length >= sb.st_size)
            {
                transferFlag = PLDM_END;
                data->length = sb.st_size - offset; // Revise length
            }
            else
            {
                transferFlag = PLDM_MIDDLE;
            }

            uint8_t* buffer = (uint8_t*)malloc(data->length);
            if (buffer == nullptr)
            {
                error(
                    "Failed to allocate buffer for http boot, length={LENGTH}",
                    "LENGTH", data->length);
                close(fd);
                return PLDM_ERROR;
            }

            ret = ::read(fd, buffer, data->length);
            if (ret < 0)
            {
                error(
                    "Failed to read file content at offset={OFFSET} of length={LENGTH} on Http boot certification file",
                    "OFFSET", offset, "LENGTH", data->length);
                free(buffer);
                close(fd);
                return PLDM_ERROR;
            }

            memcpy(pldm_oem_meta_file_io_read_resp_data(data), buffer,
                   data->length);
            free(buffer);

            offset = offset + data->length;

            data->info.data.transferFlag = transferFlag;
            data->info.data.offset = offset;

            close(fd);
            return PLDM_SUCCESS;
        }
        break;
        default:
            error("Get invalid http boot option, option={OPTION}", "OPTION",
                  data->option);
            close(fd);
            return PLDM_ERROR;
    }

noCertificationFile:
    close(fd);
    data->length = 0;
    return PLDM_SUCCESS;
}

} // namespace pldm::responder::oem_meta
