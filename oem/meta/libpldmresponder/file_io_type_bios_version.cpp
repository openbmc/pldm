#include "file_io_type_bios_version.hpp"

#include <phosphor-logging/lg2.hpp>

#include <sstream>
#include <string>

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

    uint64_t slot;
    try
    {
        slot = getSlotNumberByTID(configurations, tid);
    }
    catch (const std::exception& e)
    {
        error(
            "Fail to get the corresponding slot with TID {TID} and error code {ERROR}",
            "TID", tid, "ERROR", e);
        return PLDM_ERROR;
    }

    std::string slotNum = std::to_string(slot);
    pldm::utils::DBusMapping dbusMapping{
        std::string("/xyz/openbmc_project/software/host") + slotNum +
            "/Sentinel_Dome_bios",
        "xyz.openbmc_project.Software.Version", "Version", "string"};

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
