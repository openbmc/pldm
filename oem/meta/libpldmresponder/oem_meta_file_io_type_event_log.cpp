#include "oem_meta_file_io_type_event_log.hpp"

#include <phosphor-logging/lg2.hpp>

#include <utility>

PHOSPHOR_LOG2_USING;
namespace pldm::responder::oem_meta
{

uint8_t MAX_EVENT_LEN = 5;

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

    static constexpr auto pmic_event_type = std::to_array({
        " SWAout OV",
        " SWBout OV",
        " SWCout OV",
        " SWDout OV",
        " Vin Bulk OV",
        " Vin Mgmt OV",
        " SWAout UV",
        " SWBout UV",
        " SWCout UV",
        " SWDout UV",
        " Vin Bulk UV",
        " Vin Mgmt To Vin Buck Switchover",
        " High Temp Warning",
        " Vout 1v8 PG",
        " High Current Warning",
        " Current Limit Warning",
        " Critical Temp Shutdown",
    });

    static constexpr auto dimm_id = std::to_array({
        " DIMM A",
        " DIMM B",
        " DIMM C",
        " DIMM D",
        " DIMM E",
        " DIMM F",
        " DIMM G",
        " DIMM H",
        " DIMM I",
        " DIMM J",
        " DIMM K",
        " DIMM L",
    });

    static constexpr auto vr_source = std::to_array({
        " PVDDCR_CPU0",
        " PVDDCR_SOC",
        " PVDDCR_CPU1",
        " PVDDIO",
        " PVDD11_S3",
    });

    /* 1. Get host slot number */
    uint64_t slot;

    if (eventData.size() != MAX_EVENT_LEN)
    {
        error("{FUNC}: Invalid incoming data for event log, data size={SIZE}",
              "FUNC", std::string(__func__), "SIZE", eventData.size());
        return PLDM_ERROR;
    }

    if (eventData[0] >= eventList.size())
    {
        error("{FUNC}: Invalid event type for event log, event type={TYPE}",
              "FUNC", std::string(__func__), "TYPE", eventData[0]);
        return PLDM_ERROR;
    }

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
    std::string event_name;
    uint8_t eventType = eventData[0];
    uint8_t eventStatus = eventData[1];

    if (strcmp(eventList[eventType], " DIMM PMIC Error") == 0)
    {
        if ((eventData[2] >= dimm_id.size()) ||
            (eventData[3] >= pmic_event_type.size()))
        {
            error("{FUNC}: Invalid dimm id={ID}, pmic event type={TYPE}",
                  "FUNC", std::string(__func__), "ID", eventData[2], "TYPE",
                  eventData[3]);
            return PLDM_ERROR;
        }

        event_name = eventList[eventType] + std::string(" :") +
                     dimm_id[eventData[2]] + pmic_event_type[eventData[3]];
    }
    else if ((strcmp(eventList[eventType], " PMALERT Assertion") == 0) ||
             (strcmp(eventList[eventType], " VR Fault") == 0))
    {
        if (eventData[2] >= vr_source.size())
        {
            error("{FUNC}: Invalid vr source={VR_SOURCE}", "FUNC",
                  std::string(__func__), "VR_SOURCE", eventData[2]);
            return PLDM_ERROR;
        }

        event_name = eventList[eventType] + std::string(" :") +
                     vr_source[eventData[2]] + " status: 0x" +
                     std::format("{:02x}", eventData[3]) +
                     std::format("{:02x}", eventData[4]);
    }
    else
    {
        event_name = eventList[eventType];
    }

    if (eventStatus == EVENT_ASSERTED)
    {
        errorLog = "Event: Host" + std::to_string(slot) + event_name +
                   ", ASSERTED";
    }
    else if (eventStatus == EVENT_DEASSERTED)
    {
        errorLog = "Event: Host" + std::to_string(slot) + event_name +
                   ", DEASSERTED";
    }

    /* 3. Add SEL and journal */
    lg2::error("{ERROR}", "ERROR", errorLog);   // Create log in journal
    pldm::utils::reportError(errorLog.c_str()); // Create log on Dbus

    return PLDM_SUCCESS;
}

} // namespace pldm::responder::oem_meta
