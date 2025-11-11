#include "file_io_type_bios_version.hpp"

#include "oem/meta/utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Software/Version/client.hpp>

#include <sstream>
#include <string>

using SoftwareVersion =
    sdbusplus::common::xyz::openbmc_project::software::Version;

PHOSPHOR_LOG2_USING;
namespace pldm::responder::oem_meta
{

int BIOSVersionHandler::write(const message& data)
{
    int completionCode = checkDataIntegrity(data);
    if (completionCode != PLDM_SUCCESS)
    {
        error(
            "Invalid incoming data for setting BIOS version with completion code {CC}",
            "CC", completionCode);
        return completionCode;
    }

    std::string slotNum = pldm::oem_meta::getSlotNumberStringByTID(tid);
    pldm::utils::DBusMapping dbusMapping{
        std::format("{}/host{}/Sentinel_Dome_bios",
                    SoftwareVersion::namespace_path, slotNum),
        SoftwareVersion::interface, SoftwareVersion::property_names::version,
        "string"};

    try
    {
        dBusIntf->setDbusProperty(dbusMapping, convertToBIOSVersionStr(data));
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Failed to execute Dbus call with error code {ERROR}", "ERROR",
              e);
    }
    catch (const std::exception& e)
    {
        error("Failed to set BIOS version with error code {ERROR}", "ERROR", e);
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

int BIOSVersionHandler::checkDataIntegrity(const message& data)
{
    if (data.empty())
    {
        return PLDM_ERROR_INVALID_DATA;
    }
    else
    {
        return PLDM_SUCCESS;
    }
}

pldm::utils::PropertyValue BIOSVersionHandler::convertToBIOSVersionStr(
    const message& data)
{
    std::string biosVersion(data.begin(), data.end());

    return pldm::utils::PropertyValue{biosVersion};
}

} // namespace pldm::responder::oem_meta
