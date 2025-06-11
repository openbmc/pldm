#include "file_io_type_event_log.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>

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
                warning(
                    "GetAncestors call failed with, ERROR={ERROR} PATH={PATH} INTERFACE={INTERFACE}",
                    "ERROR", e, "PATH", configDbusPath.c_str(), "INTERFACE",
                    slotInterface);
            }

            // It should be only one of the slot property existed.
            if (response.size() != 1)
            {
                warning("Get invalide number of slot property, SIZE={SIZE}",
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
                warning(
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
        " CXL1 HB",
        " CXL2 HB",
        " Post-Started",
        " Post-Ended",
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
        " DIMM A0",
        " DIMM A1",
        " DIMM A2",
        " DIMM A3",
        " DIMM A4",
        " DIMM A5",
        " DIMM A6",
        " DIMM A7",
        " DIMM A8",
        " DIMM A9",
        " DIMM A10",
        " DIMM A11",
    });

    static constexpr auto vr_source = std::to_array<VRSource>({
        {" PVDDCR_CPU0", true},
        {" PVDDCR_SOC", true},
        {" PVDDCR_CPU1", true},
        {" PVDDIO", true},
        {" PVDD11_S3", true},
        {" PVDDQ_AB_ASIC1", true},
        {" P0V85_ASIC1", true},
        {" PVDDQ_CD_ASIC1", true},
        {" P0V8_ASIC1", true},
        {" PVDDQ_AB_ASIC2", true},
        {" P0V85_ASIC2", true},
        {" PVDDQ_CD_ASIC2", true},
        {" P0V8_ASIC2", true},
        {" P1V5_RETIMER_1", false},
        {" P0V9_STBY_1", false},
        {" P3V3_E1S_0", false},
        {" P3V3_E1S_1", false},
        {" P12V_E1S_0", false},
        {" P12V_E1S_1", false},
    });

    /* systemd service to kick start a target. */
    constexpr auto SYSTEMD_SERVICE = "org.freedesktop.systemd1";
    constexpr auto SYSTEMD_ROOT = "/org/freedesktop/systemd1";
    constexpr auto SYSTEMD_INTERFACE = "org.freedesktop.systemd1.Manager";

    /* 1. Get host slot number */
    uint64_t slot = 0;

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
        slot=0;
        warning("Using default slot0 TID={TID}, WARNING={WARNING}",
                "TID", tid, "WARNING", e);
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

        if (vr_source[eventData[2]].isPMBus)
        {
            event_name = eventList[eventType] + std::string(" :") +
                  vr_source[eventData[2]].netName + " status: 0x" +
                  std::format("{:02x}", eventData[3]) +
                  std::format("{:02x}", eventData[4]);
        }
        else
        {
            event_name = eventList[eventType] + std::string(" :") +
                  vr_source[eventData[2]].netName;
        }
    }
    else if ((strcmp(eventList[eventType], " Post-Completed") == 0))
    {
        /* The retimer is not available temporary after fw update.
           We should update fw version on d-bus when post-complete. */
        auto bus = sdbusplus::bus::new_default();
        auto method = bus.new_method_call(SYSTEMD_SERVICE, SYSTEMD_ROOT,
                                          SYSTEMD_INTERFACE, "StartUnit");
        auto service = std::string("fw-versions-sd-retimer@") +
                       std::to_string(slot) + ".service";
        method.append(service, "replace");
        bus.call_noreply(method);

        event_name = eventList[eventType];
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
    constexpr auto loggingService = "xyz.openbmc_project.Logging";
    constexpr auto loggingPath = "/xyz/openbmc_project/logging";
    constexpr auto loggingInterface = "xyz.openbmc_project.Logging.Create";
    if (event_name == " CXL1 HB" || event_name == " CXL2 HB")
    {
        lg2::info("{INFO}", "INFO", errorLog); // Create log in journal
        auto& bus = pldm::utils::DBusHandler::getBus();
        using EntrySeverity =
            sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level;

        try
        {
            auto severityStr = sdbusplus::xyz::openbmc_project::Logging::
                server::convertForMessage(EntrySeverity::Informational);

            auto method = bus.new_method_call(loggingService, loggingPath,
                                              loggingInterface, "Create");

            std::map<std::string, std::string> addlData{};
            method.append(errorLog.c_str(), severityStr, addlData);
            bus.call_noreply(method, dbusTimeout);
        }
        catch (const std::exception& e)
        {
            error("Failed to create info-level D-Bus log: {ERR}", "ERR", e);
        }
    }
    else
    {
        lg2::error("{ERROR}", "ERROR", errorLog);   // Create log in journal
        pldm::utils::reportError(errorLog.c_str()); // Create log on Dbus
    }

    return PLDM_SUCCESS;
}

} // namespace pldm::responder::oem_meta
