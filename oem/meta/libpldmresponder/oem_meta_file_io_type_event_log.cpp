#include "oem_meta_file_io_type_event_log.hpp"

#include <phosphor-logging/lg2.hpp>

#include <utility>

PHOSPHOR_LOG2_USING;
namespace pldm::responder::oem_meta
{

void EventLogHandler::parseObjectPathToGetSlot(uint64_t& slotNum)
{
    static constexpr auto slotInterface =
        "xyz.openbmc_project.Inventory.Decorator.Slot";
    static constexpr auto slotNumberProperty = "SlotNumber";

    std::vector<std::string> slotInterfaceList = {slotInterface};
    pldm::utils::GetAncestorsResponse response;
    bool found = false;

    for (const auto& [configDbusPath, mctpEndpoint] : configurations)
    {
        if (mctpEndpoint.EndpointId == tid)
        {
            try
            {
                response = pldm::utils::DBusHandler().getAncestors(
                    configDbusPath.c_str(), slotInterfaceList);
            }
            catch (const sdbusplus::exception_t& e)
            {
                error(
                    "GetAncestors call failed with, ERROR={ERROR} PATH={PATH} INTERFACE={INTERFACE}",
                    "ERROR", e, "PATH", configDbusPath.c_str(), "INTERFACE",
                    slotInterface);
            }

            // It should be only one of the slot property existed.
            if (response.size() != 1)
            {
                error("Get invalide number of slot property, SIZE={SIZE}",
                      "SIZE", response.size());
                return;
            }

            try
            {
                slotNum = pldm::utils::DBusHandler().getDbusProperty<uint64_t>(
                    std::get<0>(response.front()).c_str(), slotNumberProperty,
                    slotInterface);
                found = true;
            }
            catch (const sdbusplus::exception_t& e)
            {
                error(
                    "Error getting Names property, ERROR={ERROR} PATH={PATH} INTERFACE={INTERFACE}",
                    "ERROR", e, "PATH", std::get<0>(response.front()).c_str(),
                    "INTERFACE", slotInterface);
            }
        }
    }

    if (!found)
    {
        throw std::invalid_argument("Configuration of TID not found.");
    }
}

int EventLogHandler::write(const message& eventData)
{
    static constexpr auto eventList = std::to_array({
        " CPU Thermal Trip",
        " HSC OCP",
        " P12V_STBY UV",
        " PMALERT Assertion",
        " FAST_PROCHOT Assertion",
        " FRB3 Timer Expired",
        " Power-On Sequence Failure",
        " DIMM PMIC Error",
        " ADDC dump",
        " BMC comes out of cold reset",
        " BIOS FRB2 Watchdog Timer Expired",
        " BIC Power Failure",
        " CPU Power Failure",
        " v-boot Failure",
        " BMC is requested to reboot",
        " Chassis power on is initiated by NIC insertion",
        " Blade is powered cycle by blade button",
        " Chassis is powered cycle by sled button",
        " HSC Fault",
        " SYS Throttle",
        " VR Fault",
        " System Management Error",
        " Post-Completed",
        " Fan Error",
        " HDT_PRSNT Assertion",
        " PLTRST Assertion",
        " APML_ALERT Assertion",
    });

    /* 1. Get host slot number */
    uint64_t slot;

    try
    {
        parseObjectPathToGetSlot(slot);
    }
    catch (const std::exception& e)
    {
        error("Fail to get the corresponding slot. TID={TID}, ERROR={ERROR}",
              "TID", tid, "ERROR", e);
        return PLDM_ERROR;
    }

    /* 2. Map event from list */
    std::string errorLog;
    uint8_t eventType = eventData[0];
    uint8_t eventStatus = eventData[1];

    if (eventStatus == EVENT_ASSERTED)
    {
        errorLog = "Event: Host" + std::to_string(slot) + eventList[eventType] +
                   ", ASSERTED";
    }
    else if (eventStatus == EVENT_DEASSERTED)
    {
        errorLog = "Event: Host" + std::to_string(slot) + eventList[eventType] +
                   ", DEASSERTED";
    }

    /* 3. Add SEL and journal */
    lg2::error("{ERROR}", "ERROR", errorLog);   // Create log in journal
    pldm::utils::reportError(errorLog.c_str()); // Create log on Dbus

    return PLDM_SUCCESS;
}

} // namespace pldm::responder::oem_meta
