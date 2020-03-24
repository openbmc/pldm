#include "file_io_type_cert.hpp"

#include "utils.hpp"

#include <stdint.h>

#include <iostream>

#include "libpldm/base.h"
#include "oem/ibm/libpldm/file_io.h"

namespace pldm
{
namespace responder
{

static constexpr auto csrFilePath = "/var/lib/bmcweb/CSR";
static constexpr auto rootCertPath = "/var/lib/bmcweb/RootCert";
static constexpr auto clientCertPath = "/var/lib/bmcweb/ClientCert";

CertMap CertHandler::certMap;

int CertHandler::writeFromMemory(uint32_t offset, uint32_t length,
                                 uint64_t address)
{
    auto it = certMap.find(this->certType);
    if (it == certMap.end())
    {
        std::cerr << "Either the newFileAvailable request was not received for"
                  << " this or the file did not exist\n";
        return PLDM_ERROR;
    }
    auto certDetails = it->second;

    auto rc = transferFileData(std::get<0>(certDetails), false, offset, length,
                               address);
    if (rc == PLDM_SUCCESS)
    {
        auto remSize = std::get<1>(certDetails) - length;
        if (remSize)
        {
            it->second = std::tuple(std::get<0>(certDetails), remSize);
        }
        else
        {
            close(std::get<0>(certDetails));
            certMap.erase(it);
        }
    }
    return rc;
}

int CertHandler::readIntoMemory(uint32_t offset, uint32_t& length,
                                uint64_t address)
{
    if (this->certType != PLDM_FILE_TYPE_CERT_SIGNING_REQUEST)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }
    return transferFileData(csrFilePath, true, offset, length, address);
}

int CertHandler::read(uint32_t offset, uint32_t& length, Response& response)
{
    if (this->certType != PLDM_FILE_TYPE_CERT_SIGNING_REQUEST)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }
    return readFile(csrFilePath, offset, length, response);
}

int CertHandler::write(const char* buffer, uint32_t offset, uint32_t& length)
{
    auto it = certMap.find(this->certType);
    if (it == certMap.end())
    {
        std::cerr << "Either the newFileAvailable request was not received for"
                  << " this or the file did not exist\n";
        return PLDM_ERROR;
    }
    auto certDetails = it->second;

    int rc = lseek(std::get<0>(certDetails), offset, SEEK_SET);
    if (rc == -1)
    {
        std::cerr << "lseek failed, ERROR=" << errno << ", OFFSET=" << offset
                  << "\n";
        return PLDM_ERROR;
    }
    rc = ::write(std::get<0>(certDetails), buffer, length);
    if (rc == -1)
    {
        std::cerr << "file write failed, ERROR=" << errno
                  << ", LENGTH=" << length << ", OFFSET=" << offset << "\n";
        return PLDM_ERROR;
    }
    close(std::get<0>(certDetails));
    certMap.erase(it);
    return PLDM_SUCCESS;
}

int CertHandler::newFileAvailable(uint64_t length)
{
    int fileFd = -1;
    int flags = O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE;

    if (this->certType == PLDM_FILE_TYPE_CERT_SIGNING_REQUEST)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }
    if (this->certType == PLDM_FILE_TYPE_SIGNED_CERT)
    {
        fileFd = open(clientCertPath, flags);
    }
    if (this->certType == PLDM_FILE_TYPE_ROOT_CERT)
    {
        fileFd = open(rootCertPath, flags);
    }
    if (fileFd == -1)
    {
        std::cerr << "Signed certificate does not exist for certificate type"
                  << this->certType << " ERROR=" << errno << "\n";
        return PLDM_ERROR;
    }
    certMap.emplace(this->certType, std::tuple(fileFd, length));
    return PLDM_SUCCESS;
}

} // namespace responder
} // namespace pldm
