#include "file_io_type_lic.hpp"

#include "libpldm/base.h"
#include "oem/ibm/libpldm/file_io.h"

#include "common/utils.hpp"

#include <stdint.h>

#include <iostream>

namespace pldm
{
using namespace utils;

namespace responder
{

static constexpr auto codFilePath = "/var/lib/ibm/cod/";

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

int LicenseHandler::fileAckWithMetaData(uint32_t metaDataValue)
{
    constexpr auto codLicInterface = "com.ibm.License.LicenseManager";
    constexpr auto codLicObjPath = "/com/ibm/license";

    Status status = static_cast<Status>(metaDataValue);

    if (status == Status::InvalidLicense)
    {
        PropertyValue valueStatus{
            "com.ibm.License.LicenseManager.Status.InvalidLicense"};
        DBusMapping dbusMappingStatus{codLicObjPath, codLicInterface, "Status",
                                      "string"};

        try
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMappingStatus,
                                                       valueStatus);
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
        PropertyValue valueStatus{
            "com.ibm.License.LicenseManager.Status.Activated"};
        DBusMapping dbusMappingStatus{codLicObjPath, codLicInterface, "Status",
                                      "string"};

        try
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMappingStatus,
                                                       valueStatus);
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
        PropertyValue valueStatus{
            "com.ibm.License.LicenseManager.Status.Pending"};
        DBusMapping dbusMappingStatus{codLicObjPath, codLicInterface, "Status",
                                      "string"};

        try
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMappingStatus,
                                                       valueStatus);
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
        PropertyValue valueStatus{
            "com.ibm.License.LicenseManager.Status.ActivationFailed"};
        DBusMapping dbusMappingStatus{codLicObjPath, codLicInterface, "Status",
                                      "string"};

        try
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMappingStatus,
                                                       valueStatus);
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
        PropertyValue valueStatus{
            "com.ibm.License.LicenseManager.Status.IncorrectSystem"};
        DBusMapping dbusMappingStatus{codLicObjPath, codLicInterface, "Status",
                                      "string"};

        try
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMappingStatus,
                                                       valueStatus);
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
        PropertyValue valueStatus{
            "com.ibm.License.LicenseManager.Status.InvalidHostState"};
        DBusMapping dbusMappingStatus{codLicObjPath, codLicInterface, "Status",
                                      "string"};

        try
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMappingStatus,
                                                       valueStatus);
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
        PropertyValue valueStatus{
            "com.ibm.License.LicenseManager.Status.IncorrectSequence"};
        DBusMapping dbusMappingStatus{codLicObjPath, codLicInterface, "Status",
                                      "string"};

        try
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMappingStatus,
                                                       valueStatus);
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
