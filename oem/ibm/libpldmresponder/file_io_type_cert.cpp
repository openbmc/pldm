#include "file_io_type_cert.hpp"

#include "libpldm/base.h"
#include "oem/ibm/libpldm/file_io.h"

#include "common/utils.hpp"

#include <stdint.h>

#include <iostream>

namespace pldm
{
namespace responder
{

CertMap CertHandler::certMap;

int CertHandler::writeFromMemory(uint32_t offset, uint32_t length,
                                 uint64_t address,
                                 oem_platform::Handler* /*oemPlatformHandler*/)
{
    auto it = certMap.find(certType);
    if (it == certMap.end())
    {
        std::cerr << "file for type " << certType << " doesn't exist\n";
        return PLDM_ERROR;
    }

    auto fd = std::get<0>(it->second);
    auto& remSize = std::get<1>(it->second);
    auto rc = transferFileData(fd, false, offset, length, address);
    if (rc == PLDM_SUCCESS)
    {
        remSize -= length;
        if (!remSize)
        {
            close(fd);
            certMap.erase(it);
        }
    }
    return rc;
}

int CertHandler::readIntoMemory(uint32_t offset, uint32_t& length,
                                uint64_t address,
                                oem_platform::Handler* /*oemPlatformHandler*/)
{
    if (certType != PLDM_FILE_TYPE_CERT_SIGNING_REQUEST)
    {
        return PLDM_ERROR_INVALID_DATA;
    }
    return transferFileData(csrFilePath, true, offset, length, address);
}

int CertHandler::read(uint32_t offset, uint32_t& length, Response& response,
                      oem_platform::Handler* /*oemPlatformHandler*/)
{
    if (certType != PLDM_FILE_TYPE_CERT_SIGNING_REQUEST)
    {
        return PLDM_ERROR_INVALID_DATA;
    }
    return readFile(csrFilePath, offset, length, response);
}

int CertHandler::write(const char* buffer, uint32_t offset, uint32_t& length,
                       oem_platform::Handler* /*oemPlatformHandler*/)
{
    auto it = certMap.find(certType);
    if (it == certMap.end())
    {
        std::cerr << "file for type " << certType << " doesn't exist\n";
        return PLDM_ERROR;
    }

    auto fd = std::get<0>(it->second);
    int rc = lseek(fd, offset, SEEK_SET);
    if (rc == -1)
    {
        std::cerr << "lseek failed, ERROR=" << errno << ", OFFSET=" << offset
                  << "\n";
        return PLDM_ERROR;
    }
    rc = ::write(fd, buffer, length);
    if (rc == -1)
    {
        std::cerr << "file write failed, ERROR=" << errno
                  << ", LENGTH=" << length << ", OFFSET=" << offset << "\n";
        return PLDM_ERROR;
    }
    length = rc;
    auto& remSize = std::get<1>(it->second);
    remSize -= length;
    if (!remSize)
    {
        close(fd);
        certMap.erase(it);
    }
    return PLDM_SUCCESS;
}

int CertHandler::newFileAvailable(uint64_t length)
{
    static constexpr auto vmiCertPath =
        "/var/lib/bmcweb" + "/cert_" + fileHandle;
    fs::create_directories(vmiCertPath);
    int fileFd = -1;
    int flags = O_WRONLY | O_CREAT | O_TRUNC;

    if (certType == PLDM_FILE_TYPE_CERT_SIGNING_REQUEST)
    {
        return PLDM_ERROR_INVALID_DATA;
    }
    if (certType == PLDM_FILE_TYPE_SIGNED_CERT)
    {
        vmiCertPath += "/ClientCert";
        fileFd = open(vmiCertPath, flags, S_IRUSR | S_IWUSR);
    }
    else if (certType == PLDM_FILE_TYPE_ROOT_CERT)
    {
        vmiCertPath += "/RootCert";
        fileFd = open(vmiCertPath, flags, S_IRUSR | S_IWUSR);
    }
    if (fileFd == -1)
    {
        std::cerr << "failed to open file for type " << certType
                  << " ERROR=" << errno << "\n";
        return PLDM_ERROR;
    }
    certMap.emplace(certType, std::tuple(fileFd, length));
    return PLDM_SUCCESS;
}

} // namespace responder
} // namespace pldm
