#include "oem_meta_file_io_type_event_log.hpp"

#include <phosphor-logging/commit.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/State/Power/event.hpp>
#include <xyz/openbmc_project/State/Thermal/event.hpp>

#include <utility>

PHOSPHOR_LOG2_USING;
namespace pldm::responder::oem_meta
{

uint8_t MAX_EVENT_LEN = 5;

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
    " PROCHOT is triggered due to SD sensor reading exceed UCR",
    " FRB2 WDT HARD RST",
    " FRB2 WDT PWR DOWN",
    " FRB2 WDT PWR CYCLE",
    " OS LOAD WDT EXPIRED",
    " OS LOAD WDT HARD_RST",
    " OS LOAD WDT PWR_DOWN",
    " OS LOAD WDT PWR_CYCLE",
    " MTIA FAULT",
    " Post-Timeouted",
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
static auto vr_source = std::to_array<VRSource>({
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
    {" PVTT_AB_ASIC1", false},
    {" PVTT_AB_ASIC2", false},
    {" PVTT_CD_ASIC1", false},
    {" PVTT_CD_ASIC2", false},
    {" PVPP_AB_ASIC1", false},
    {" PVPP_AB_ASIC2", false},
    {" PVPP_CD_ASIC1", false},
    {" PVPP_CD_ASIC2", false},
});

const std::vector<FaultDesc> faultsList = {
    {" MTIA FAULT", 0x00, "MTIA_P3V3",                        "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p3v3", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x01, "MTIA_P0V85_PVDD",                  "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v85_pvdd", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x02, "MTIA_P0V75_PVDD_CH_N",             "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_pvdd_ch_n", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x03, "MTIA_P0V75_MAX_PHY_N",             "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_max_phy_n", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x04, "MTIA_P0V75_PVDD_CH_S",             "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_pvdd_ch_s", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x05, "MTIA_P0V75_MAX_PHY_S",             "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_max_phy_s", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x06, "MTIA_P0V75_TRVDD_ZONEA",           "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_trvdd_zonea", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x07, "MTIA_P1V8_VPP_HBM0_HBM2_HBM4",     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p1v8_vpp_hbm0_hbm2_hbm4", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x08, "MTIA_P0V75_TRVDD_ZONEB",           "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_trvdd_zoneb", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x09, "MTIA_P0V4_VDDQL_HBM0_HBM2_HBM4",   "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v4_vddql_hbm0_hbm2_hbm4", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x0A, "MTIA_P1V1_VDDC_HBM0_HBM2_HBM4",    "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p1v1_vddc_hbm0_hbm2_hbm4", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x0B, "MTIA_P0V75_VDDPHY_HBM0_HBM2_HBM4", "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_vddphy_hbm0_hbm2_hbm4", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x0C, "MTIA_P0V9_TRVDD_ZONEA",            "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v9_trvdd_zonea", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x0D, "MTIA_P1V8_VPP_HBM1_HBM3_HBM5",     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p1v8_vpp_hbm1_hbm3_hbm5", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x0E, "MTIA_P0V9_TRVDD_ZONEB",            "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v9_trvdd_zoneb", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x0F, "MTIA_P0V4_VDDQL_HBM1_HBM3_HBM5",   "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v4_vddql_hbm1_hbm3_hbm5", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x10, "MTIA_P1V1_VDDC_HBM1_HBM3_HBM5",    "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p1v1_vddc_hbm1_hbm3_hbm5", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x11, "MTIA_P0V75_VDDPHY_HBM1_HBM3_HBM5", "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_vddphy_hbm1_hbm3_hbm5", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x12, "MTIA_P0V8_VDDA_PCIE",              "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v8_vdda_pcie", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x13, "MTIA_P1V2_VDDHTX_PCIE",            "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p1v2_vddhtx_pcie", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x14, "MTIA_P12V_UBC1",                   "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p12v_ubc1", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x15, "MTIA_P12V_UBC2",                   "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p12v_ubc2", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x16, "MTIA_VR_MAX",                      "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/vr_max", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x30, "MTIA_PLL_VDDA15_PCIE_MAX_CORE",    "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/pll_vdda15_pcie_max_core", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x31, "MTIA_P0V75_AVDD_HSCL",             "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_avdd_hscl", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x32, "MTIA_P0V75_VDDC_CLKOBS",           "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_vddc_clkobs", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x33, "MTIA_PLL_VDDA15_MAX_CORE_N",       "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/pll_vdda15_max_core_n", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x34, "MTIA_PVDD1P5_S",                   "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/pvdd1p5_s", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x35, "MTIA_PVDD1P5_N",                   "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/pvdd1p5_n", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x36, "MTIA_PVDD0P9_S",                   "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/pvdd0p9_s", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x37, "MTIA_PVDD0P9_N",                   "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/pvdd0p9_n", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x38, "MTIA_PLL_VDDA15_HBM0_HBM2_HBM4",   "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/pll_vdda15_hbm0_hbm2_hbm4", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x39, "MTIA_PLL_VDDA15_HBM1_HBM3_HBM5",   "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/pll_vdda15_hbm1_hbm3_hbm5", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x3A, "MTIA_P3V3_OSC",                    "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p3v3_osc", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x3B, "MTIA_P5V",                         "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p5v", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x3C, "MTIA_LDO_IN_1V8",                  "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/ldo_in_1v8", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x3D, "MTIA_LDO_IN_1V2",                  "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/ldo_in_1v2", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x3E, "MTIA_PLL_VDDA15_MAX_CORE_S",       "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/pll_vdda15_max_core_s", "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x50, "MTIA_POWER_ON_SEQUENCE_FAIL",      "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/asic0", "PowerRailFault"},
    {" MTIA FAULT", 0x51, "MTIA_FM_ASIC_0_THERMTRIP_N",       "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/asic_0_thermtrip_n", "DeviceOverOperatingTemperatureFault"},
    {" MTIA FAULT", 0x52, "MTIA_FM_ATH_PLD_HBM3_CATTRIP",     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/ath_pld_hbm3_cattrip", "DeviceOverOperatingTemperatureFault"},
};

const FaultDesc* lookupFaultByIndex(uint8_t index)
{
    auto it = std::find_if(faultsList.begin(), faultsList.end(),
                           [index](const auto& fault) {
                               return fault.index == index;
                           });
    return it != faultsList.end() ? &(*it) : nullptr;
}

void recordEventLog(const FaultDesc& fault, uint8_t eventStatus, const FaultData& data)
{
    if (fault.eventName == "PowerRailFault")
    {
        using namespace sdbusplus::error::xyz::openbmc_project::state::Power;
        using namespace sdbusplus::event::xyz::openbmc_project::state::Power;


        if (eventStatus == EVENT_ASSERTED)
        {
            lg2::commit(PowerRailFault(
                PowerRailFault::metadata_t<"POWER_RAIL">{"POWER_RAIL"},
                fault.object_path,
                PowerRailFault::metadata_t<"FAILURE_DATA">{"FAILURE_DATA"},
                std::format("{} - [{:02X} {:02X}]", fault.name, data.detail1, data.detail2)
            ));
        }
        else if (eventStatus == EVENT_DEASSERTED)
        {
            lg2::commit(PowerRailFaultRecovered(
                PowerRailFaultRecovered::metadata_t<"POWER_RAIL">{"POWER_RAIL"},
                fault.object_path
            ));
        }
    }
    else if (fault.eventName == "VoltageRegulatorFault")
    {
        using namespace sdbusplus::error::xyz::openbmc_project::state::Power;
        using namespace sdbusplus::event::xyz::openbmc_project::state::Power;
        
        if (eventStatus == EVENT_ASSERTED)
        {
            lg2::commit(VoltageRegulatorFault(
                VoltageRegulatorFault::metadata_t<"VOLTAGE_REGULATOR">{"VOLTAGE_REGULATOR"},
                fault.object_path,
                VoltageRegulatorFault::metadata_t<"FAILURE_DATA">{"FAILURE_DATA"},
                std::format("{} - [{:02X} {:02X}]", fault.name, data.detail1, data.detail2)
            ));
        }
        else if (eventStatus == EVENT_DEASSERTED)
        {
            lg2::commit(VoltageRegulatorFaultRecovered(
                VoltageRegulatorFaultRecovered::metadata_t<"VOLTAGE_REGULATOR">{"VOLTAGE_REGULATOR"},
                fault.object_path
            ));
        }
    }
    else if (fault.eventName == "DeviceOverOperatingTemperatureFault")
    {
        using namespace sdbusplus::error::xyz::openbmc_project::state::Thermal;
        using namespace sdbusplus::event::xyz::openbmc_project::state::Thermal;

        if (eventStatus == EVENT_ASSERTED)
        {
            lg2::commit(DeviceOverOperatingTemperatureFault(
                DeviceOverOperatingTemperatureFault::metadata_t<"DEVICE">{"DEVICE"},
                fault.object_path,
                DeviceOverOperatingTemperatureFault::metadata_t<"FAILURE_DATA">{"FAILURE_DATA"},
                std::format("{} - [{:02X} {:02X}]", fault.name, data.detail1, data.detail2)
            ));
        }
        else if (eventStatus == EVENT_DEASSERTED)
        {
            lg2::commit(DeviceOperatingNormalTemperature(
                DeviceOperatingNormalTemperature::metadata_t<"DEVICE">{"DEVICE"},
                fault.object_path
            ));
        }
    }
}

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
    /* systemd service to kick start a target. */
    constexpr auto SYSTEMD_SERVICE = "org.freedesktop.systemd1";
    constexpr auto SYSTEMD_ROOT = "/org/freedesktop/systemd1";
    constexpr auto SYSTEMD_INTERFACE = "org.freedesktop.systemd1.Manager";

    /* 1. Get host slot number */
    uint64_t slot=0;

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

    /* 1. Get host slot number */
    if (!configurations.empty())
    {
        try
        {
            parseObjectPathToGetSlot(slot);
        }
        catch (const std::exception& e)
        {
            error("Using default slot0 TID={TID}, WARNING={WARNING}",
                    "TID", tid, "WARNING", e);
            return PLDM_ERROR;
        }
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
    else if ((strcmp(eventList[eventType], " Post-Completed") == 0) || 
        (strcmp(eventList[eventType], " Post-Ended") == 0))
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
    else if (strcmp(eventList[eventType], " MTIA FAULT") == 0)
    {
        const auto index = eventData[2];
        const auto* faultEvent = lookupFaultByIndex(index);

        if (!faultEvent)
        {
            error("{FUNC}: Invalid MTIA fault source=0x{SOURCE:X}",
                "FUNC", std::string(__func__),
                "SOURCE", static_cast<int>(eventData[2]));
            return PLDM_ERROR;
        }

        FaultData data{eventData[3], eventData[4]};
        recordEventLog(*faultEvent, eventStatus, data);
        return PLDM_SUCCESS;
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
        using EntrySeverity = sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level;

        try
        {
            auto severityStr =
                sdbusplus::xyz::openbmc_project::Logging::server::convertForMessage(
                    EntrySeverity::Informational);

            auto method = bus.new_method_call(loggingService, loggingPath, loggingInterface, "Create");

            std::map<std::string, std::string> addlData{};
            method.append(errorLog.c_str(), severityStr, addlData);
            bus.call_noreply(method, dbusTimeout);
        }
        catch (const std::exception& e)
        {
            error("Failed to create info-level D-Bus log: {ERR}", "ERR", e);
        }
    }
    else if (event_name ==
             " PROCHOT is triggered due to SD sensor reading exceed UCR")
    {
        std::string device = "/xyz/openbmc_project/State/Thermal/host" +
                             std::to_string(slot) + "/cpu0";
        auto oemData = "0x" +
                       std::format("{:02x}{:02x}", eventData[3], eventData[4]);
        std::string failureData =
            "SD sensor reading exceed UCR, data: " + oemData;

        if (oemData == "0x0000")
        {
            using DeviceNormalTemp = sdbusplus::event::xyz::openbmc_project::
                state::Thermal::DeviceOperatingNormalTemperature;
            lg2::commit(DeviceNormalTemp(
                DeviceNormalTemp::metadata_t<"DEVICE">{"DEVICE"},
                sdbusplus::message::object_path(device)));
        }
        else
        {
            using DeviceOverTemp = sdbusplus::error::xyz::openbmc_project::
                state::Thermal::DeviceOverOperatingTemperature;
            lg2::commit(DeviceOverTemp(
                DeviceOverTemp::metadata_t<"DEVICE">{"DEVICE"},
                sdbusplus::message::object_path(device),
                DeviceOverTemp::metadata_t<"FAILURE_DATA">{"FAILURE_DATA"},
                failureData));
        }
    }
    else
    {
        lg2::error("{ERROR}", "ERROR", errorLog); // Create log in journal
        pldm::utils::reportError(errorLog.c_str()); // Create log on Dbus
    }

    return PLDM_SUCCESS;
}

} // namespace pldm::responder::oem_meta
