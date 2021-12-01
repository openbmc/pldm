#include "file_io_type_lic.hpp"

#include "libpldm/base.h"
#include "oem/ibm/libpldm/file_io.h"

#include "common/utils.hpp"

#include <stdint.h>

#include <iostream>

namespace pldm
{
using namespace pldm::responder::utils;
using namespace pldm::utils;
using Json = nlohmann::json;
const Json emptyJson{};
const std::vector<Json> emptyJsonList{};

namespace responder
{

static constexpr auto codFilePath = "/var/lib/ibm/cod/";
static constexpr auto licFilePath = "/var/lib/pldm/license/";
constexpr auto newLicenseFile = "new_license.bin";
constexpr auto newLicenseJsonFile = "new_license.json";

int LicenseHandler::updateBinFileAndLicObjs(const fs::path& newLicJsonFilePath)
{
    int rc = PLDM_SUCCESS;
    fs::path newLicFilePath(fs::path(licFilePath) / newLicenseFile);
    std::ifstream jsonFileNew(newLicJsonFilePath);

    auto dataNew = Json::parse(jsonFileNew, nullptr, false);
    if (dataNew.is_discarded())
    {
        std::cerr << "Parsing the new license json file failed, FILE="
                  << newLicJsonFilePath << "\n";
        throw InternalFailure();
    }

    // Store the json data in a file with binary format
    convertJsonToBinaryFile(dataNew, newLicFilePath);

    // Create or update the license objects
    rc = createOrUpdateLicenseObjs();
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "createOrUpdateLicenseObjs failed with rc= " << rc
                  << " \n";
        return rc;
    }
    return PLDM_SUCCESS;
}

int LicenseHandler::writeFromMemory(
    uint32_t offset, uint32_t length, uint64_t address,
    oem_platform::Handler* /*oemPlatformHandler*/)
{
    namespace fs = std::filesystem;
    if (!fs::exists(licFilePath))
    {
        fs::create_directories(licFilePath);
        fs::permissions(licFilePath,
                        fs::perms::others_read | fs::perms::owner_write);
    }
    fs::path newLicJsonFilePath(fs::path(licFilePath) / newLicenseJsonFile);
    std::ofstream licJsonFile(newLicJsonFilePath,
                              std::ios::out | std::ios::binary);
    if (!licJsonFile)
    {
        std::cerr << "license json file create error: " << newLicJsonFilePath
                  << std::endl;
        return -1;
    }

    auto rc =
        transferFileData(newLicJsonFilePath, false, offset, length, address);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "transferFileData failed with rc= " << rc << " \n";
        return rc;
    }

    if (length == licLength)
    {
        rc = updateBinFileAndLicObjs(newLicJsonFilePath);
        if (rc != PLDM_SUCCESS)
        {
            std::cerr << "updateBinFileAndLicObjs failed with rc= " << rc
                      << " \n";
            return rc;
        }
    }

    return PLDM_SUCCESS;
}

int LicenseHandler::write(const char* buffer, uint32_t /*offset*/,
                          uint32_t& length,
                          oem_platform::Handler* /*oemPlatformHandler*/)
{
    int rc = PLDM_SUCCESS;

    fs::path newLicJsonFilePath(fs::path(licFilePath) / newLicenseJsonFile);
    std::ofstream licJsonFile(newLicJsonFilePath,
                              std::ios::out | std::ios::binary | std::ios::app);
    if (!licJsonFile)
    {
        std::cerr << "license json file create error: " << newLicJsonFilePath
                  << std::endl;
        return -1;
    }

    if (buffer != nullptr)
    {
        licJsonFile.write(buffer, length);
    }
    licJsonFile.close();

    updateBinFileAndLicObjs(newLicJsonFilePath);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "updateBinFileAndLicObjs failed with rc= " << rc << " \n";
        return rc;
    }

    return rc;
}

int LicenseHandler::newFileAvailable(uint64_t length)
{
    licLength = length;
    return PLDM_SUCCESS;
}

int LicenseHandler::read(uint32_t offset, uint32_t& length, Response& response,
                         oem_platform::Handler* /*oemPlatformHandler*/)
{
    std::string filePath = codFilePath;
    filePath += "licFile";

    if (licType != PLDM_FILE_TYPE_COD_LICENSE_KEY)
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

int LicenseHandler::fileAckWithMetaData(uint32_t metaDataValue1,
                                        uint32_t /*metaDataValue2*/,
                                        uint32_t /*metaDataValue3*/,
                                        uint32_t /*metaDataValue4*/)
{
    DBusMapping dbusMapping;
    dbusMapping.objectPath = "/com/ibm/license";
    dbusMapping.interface = "com.ibm.License.LicenseManager";
    dbusMapping.propertyName = "LicenseActivationStatus";
    dbusMapping.propertyType = "string";

    Status status = static_cast<Status>(metaDataValue1);

    if (status == Status::InvalidLicense)
    {
        pldm::utils::PropertyValue value =
            "com.ibm.License.LicenseManager.Status.InvalidLicense";

        try
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMapping, value);
        }
        catch (const std::exception& e)
        {
            std::cerr << "failed to set status property of license manager, "
                         "ERROR="
                      << e.what() << "\n";
            return PLDM_ERROR;
        }
    }
    else if (status == Status::Activated)
    {
        pldm::utils::PropertyValue value =
            "com.ibm.License.LicenseManager.Status.Activated";

        try
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMapping, value);
        }
        catch (const std::exception& e)
        {
            std::cerr << "failed to set status property of license manager, "
                         "ERROR="
                      << e.what() << "\n";
            return PLDM_ERROR;
        }
    }
    else if (status == Status::Pending)
    {
        pldm::utils::PropertyValue value =
            "com.ibm.License.LicenseManager.Status.Pending";

        try
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMapping, value);
        }
        catch (const std::exception& e)
        {
            std::cerr << "failed to set status property of license manager, "
                         "ERROR="
                      << e.what() << "\n";
            return PLDM_ERROR;
        }
    }
    else if (status == Status::ActivationFailed)
    {
        pldm::utils::PropertyValue value =
            "com.ibm.License.LicenseManager.Status.ActivationFailed";

        try
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMapping, value);
        }
        catch (const std::exception& e)
        {
            std::cerr << "failed to set status property of license manager, "
                         "ERROR="
                      << e.what() << "\n";
            return PLDM_ERROR;
        }
    }
    else if (status == Status::IncorrectSystem)
    {
        pldm::utils::PropertyValue value =
            "com.ibm.License.LicenseManager.Status.IncorrectSystem";

        try
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMapping, value);
        }
        catch (const std::exception& e)
        {
            std::cerr << "failed to set status property of license manager, "
                         "ERROR="
                      << e.what() << "\n";
            return PLDM_ERROR;
        }
    }
    else if (status == Status::InvalidHostState)
    {
        pldm::utils::PropertyValue value =
            "com.ibm.License.LicenseManager.Status.InvalidHostState";

        try
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMapping, value);
        }
        catch (const std::exception& e)
        {
            std::cerr << "failed to set status property of license manager, "
                         "ERROR="
                      << e.what() << "\n";
            return PLDM_ERROR;
        }
    }
    else if (status == Status::IncorrectSequence)
    {
        pldm::utils::PropertyValue value =
            "com.ibm.License.LicenseManager.Status.IncorrectSequence";

        try
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMapping, value);
        }
        catch (const std::exception& e)
        {
            std::cerr << "failed to set status property of license manager, "
                         "ERROR="
                      << e.what() << "\n";
            return PLDM_ERROR;
        }
    }
    return PLDM_SUCCESS;
}

} // namespace responder
} // namespace pldm
