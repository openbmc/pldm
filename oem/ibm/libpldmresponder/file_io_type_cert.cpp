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

static constexpr auto csrFilePath = "/var/lib/bmcweb/CSR";
static constexpr auto rootCertPath = "/var/lib/bmcweb/RootCert";
static constexpr auto clientCertPath = "/var/lib/bmcweb/ClientCert";

CertMap CertHandler::certMap;

int CertHandler::writeFromMemory(uint32_t offset, uint32_t length,
                                 uint64_t address,
                                 oem_platform::Handler* /*oemPlatformHandler*/)
{
    std::cerr << "VMI: Inside write from memory" << std::endl;
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
    std::cerr << "VMI: Inside read into memory" << std::endl;
    if (certType != PLDM_FILE_TYPE_CERT_SIGNING_REQUEST)
    {
        return PLDM_ERROR_INVALID_DATA;
    }
    return transferFileData(csrFilePath, true, offset, length, address);
}

int CertHandler::read(uint32_t offset, uint32_t& length, Response& response,
                      oem_platform::Handler* /*oemPlatformHandler*/)
{
    std::cerr << "VMI: Inside read" << std::endl;
    if (certType != PLDM_FILE_TYPE_CERT_SIGNING_REQUEST)
    {
        return PLDM_ERROR_INVALID_DATA;
    }
    return readFile(csrFilePath, offset, length, response);
}

int CertHandler::write(const char* buffer, uint32_t offset, uint32_t& length,
                       oem_platform::Handler* /*oemPlatformHandler*/)
{
    std::cerr << "VMI: Inside write" << std::endl;
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
    static constexpr auto vmiCertPath = "/var/lib/bmcweb";

    std::cerr << "VMI: Inside new file available" << std::endl;
    fs::create_directories(vmiCertPath);
    int fileFd = -1;
    int flags = O_WRONLY | O_CREAT | O_TRUNC;

    if (certType == PLDM_FILE_TYPE_CERT_SIGNING_REQUEST)
    {
        return PLDM_ERROR_INVALID_DATA;
    }
    if (certType == PLDM_FILE_TYPE_SIGNED_CERT)
    {
        std::cerr << "VMI: Client Cert new file available" << std::endl;
        fileFd = open(clientCertPath, flags, S_IRUSR | S_IWUSR);

        static constexpr auto certObjPath = "/xyz/openbmc_project/certs/entry";
        static constexpr auto certEntryIntf = "xyz.openbmc_project.Certs.Entry";

        std::fstream inFile;
        inFile.open(clientCertPath);
        std::stringstream strStream;
        strStream << inFile.rdbuf();
        std::string str = strStream.str();

        std::cerr << "VMI: Client cert: " << str << std::endl;

        PropertyValue value{str};
        DBusMapping dbusMapping{certObjPath, certEntryIntf, "ClientCertificate",
                                "string"};
        try
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMapping, value);
        }
        catch (const std::exception& e)
        {
            std::cerr << "failed to set Client certificate, "
                         "ERROR="
                      << e.what() << "\n";
        }
        if (!str.empty())
        {
            std::cerr << "Cert Status complete" << std::endl;
            PropertyValue value{
                "xyz.openbmc_project.Certs.Entry.State.Complete"};
            DBusMapping dbusMapping{certObjPath, certEntryIntf, "Status",
                                    "string"};
            try
            {
                pldm::utils::DBusHandler().setDbusProperty(dbusMapping, value);
            }
            catch (const std::exception& e)
            {
                std::cerr
                    << "failed to set status property of certicate entry, "
                       "ERROR="
                    << e.what() << "\n";
            }
        }
    }
    else if (certType == PLDM_FILE_TYPE_ROOT_CERT)
    {
        std::cerr << "VMI: Root Cert new file available" << std::endl;
        fileFd = open(rootCertPath, flags, S_IRUSR | S_IWUSR);
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
