#include "file_io_type_event_log.hpp"

#include <phosphor-logging/commit.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/State/Power/event.hpp>
#include <xyz/openbmc_project/State/Thermal/event.hpp>

#include <utility>

PHOSPHOR_LOG2_USING;
namespace pldm::responder::oem_meta
{
uint8_t EVENT_LEN = 5;
uint8_t EVENT_WITH_TIMESTAMP_LEN = 9;

// Mapping of (tid, event_type) to the timestamp when it was sent by the BIC.
std::map<std::pair<pldm_tid_t, EventType>, uint32_t> receivedEventTimeStamp;

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
    " RAINBOW FAULT",
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

static const auto vr_source = std::to_array<VRSource>({
    {" PVDDCR_CPU0", true},    {" PVDDCR_SOC", true},
    {" PVDDCR_CPU1", true},    {" PVDDIO", true},
    {" PVDD11_S3", true},      {" PVDDQ_AB_ASIC1", true},
    {" P0V85_ASIC1", true},    {" PVDDQ_CD_ASIC1", true},
    {" P0V8_ASIC1", true},     {" PVDDQ_AB_ASIC2", true},
    {" P0V85_ASIC2", true},    {" PVDDQ_CD_ASIC2", true},
    {" P0V8_ASIC2", true},     {" P1V5_RETIMER_1", false},
    {" P0V9_STBY_1", false},   {" P3V3_E1S_0", false},
    {" P3V3_E1S_1", false},    {" P12V_E1S_0", false},
    {" P12V_E1S_1", false},    {" PVTT_AB_ASIC1", false},
    {" PVTT_AB_ASIC2", false}, {" PVTT_CD_ASIC1", false},
    {" PVTT_CD_ASIC2", false}, {" PVPP_AB_ASIC1", false},
    {" PVPP_AB_ASIC2", false}, {" PVPP_CD_ASIC1", false},
    {" PVPP_CD_ASIC2", false},
});

const std::vector<FaultDesc> mtiaFaultsList = {
    {" MTIA FAULT", 0x00, "MTIA_P3V3",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p3v3",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x01, "MTIA_P0V85_PVDD",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v85_pvdd",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x02, "MTIA_P0V75_PVDD_CH_N",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_pvdd_ch_n",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x03, "MTIA_P0V75_MAX_PHY_N",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_max_phy_n",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x04, "MTIA_P0V75_PVDD_CH_S",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_pvdd_ch_s",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x05, "MTIA_P0V75_MAX_PHY_S",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_max_phy_s",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x06, "MTIA_P0V75_TRVDD_ZONEA",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_trvdd_zonea",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x07, "MTIA_P1V8_VPP_HBM0_HBM2_HBM4",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p1v8_vpp_hbm0_hbm2_hbm4",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x08, "MTIA_P0V75_TRVDD_ZONEB",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_trvdd_zoneb",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x09, "MTIA_P0V4_VDDQL_HBM0_HBM2_HBM4",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v4_vddql_hbm0_hbm2_hbm4",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x0A, "MTIA_P1V1_VDDC_HBM0_HBM2_HBM4",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p1v1_vddc_hbm0_hbm2_hbm4",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x0B, "MTIA_P0V75_VDDPHY_HBM0_HBM2_HBM4",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_vddphy_hbm0_hbm2_hbm4",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x0C, "MTIA_P0V9_TRVDD_ZONEA",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v9_trvdd_zonea",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x0D, "MTIA_P1V8_VPP_HBM1_HBM3_HBM5",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p1v8_vpp_hbm1_hbm3_hbm5",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x0E, "MTIA_P0V9_TRVDD_ZONEB",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v9_trvdd_zoneb",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x0F, "MTIA_P0V4_VDDQL_HBM1_HBM3_HBM5",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v4_vddql_hbm1_hbm3_hbm5",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x10, "MTIA_P1V1_VDDC_HBM1_HBM3_HBM5",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p1v1_vddc_hbm1_hbm3_hbm5",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x11, "MTIA_P0V75_VDDPHY_HBM1_HBM3_HBM5",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_vddphy_hbm1_hbm3_hbm5",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x12, "MTIA_P0V8_VDDA_PCIE",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v8_vdda_pcie",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x13, "MTIA_P1V2_VDDHTX_PCIE",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p1v2_vddhtx_pcie",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x14, "MTIA_P12V_UBC1",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p12v_ubc1",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x15, "MTIA_P12V_UBC2",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p12v_ubc2",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x16, "MTIA_VR_MAX",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/vr_max",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x30, "MTIA_PLL_VDDA15_PCIE_MAX_CORE",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/pll_vdda15_pcie_max_core",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x31, "MTIA_P0V75_AVDD_HSCL",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_avdd_hscl",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x32, "MTIA_P0V75_VDDC_CLKOBS",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p0v75_vddc_clkobs",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x33, "MTIA_PLL_VDDA15_MAX_CORE_N",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/pll_vdda15_max_core_n",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x34, "MTIA_PVDD1P5_S",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/pvdd1p5_s",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x35, "MTIA_PVDD1P5_N",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/pvdd1p5_n",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x36, "MTIA_PVDD0P9_S",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/pvdd0p9_s",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x37, "MTIA_PVDD0P9_N",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/pvdd0p9_n",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x38, "MTIA_PLL_VDDA15_HBM0_HBM2_HBM4",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/pll_vdda15_hbm0_hbm2_hbm4",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x39, "MTIA_PLL_VDDA15_HBM1_HBM3_HBM5",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/pll_vdda15_hbm1_hbm3_hbm5",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x3A, "MTIA_P3V3_OSC",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p3v3_osc",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x3B, "MTIA_P5V",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/p5v",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x3C, "MTIA_LDO_IN_1V8",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/ldo_in_1v8",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x3D, "MTIA_LDO_IN_1V2",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/ldo_in_1v2",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x3E, "MTIA_PLL_VDDA15_MAX_CORE_S",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/pll_vdda15_max_core_s",
     "VoltageRegulatorFault"},
    {" MTIA FAULT", 0x50, "MTIA_POWER_ON_SEQUENCE_FAIL",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/asic0",
     "PowerRailFault"},
    {" MTIA FAULT", 0x51, "MTIA_FM_ASIC_0_THERMTRIP_N",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/asic_0_thermtrip_n",
     "DeviceOverOperatingTemperatureFault"},
    {" MTIA FAULT", 0x52, "MTIA_FM_ATH_PLD_HBM3_CATTRIP",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/ath_pld_hbm3_cattrip",
     "DeviceOverOperatingTemperatureFault"},
    {" MTIA FAULT", 0x53, "MTIA_VR_FAULT_CAUSE_POWER_DOWN",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/vr_fault_power_down",
     "PowerRailFault"},
    {" MTIA FAULT", 0x54, "MTIA_ATH_GPIO_3",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/ath_gpio_3",
     "DeviceOverOperatingTemperatureFault"},
    {" MTIA FAULT", 0x55, "MTIA_ATH_GPIO_4",
     "/xyz/openbmc_project/inventory/system/board/Minerva_Aegis/ath_gpio_4",
     "DeviceOverOperatingTemperatureFault"},
};

const std::vector<FaultDesc> rainbowFaultsList = {
    {" RAINBOW FAULT", 0x00, "RAINBOW_HAMSA_VDDHRXTX_PCIE",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/hamsa_vddhrxtx_pcie",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x01, "RAINBOW_HAMSA_AVDD_PCIE",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/hamsa_avdd_pcie",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x02, "RAINBOW_OWL_W_TRVDD0P75",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/owl_w_trvdd0p75",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x03, "RAINBOW_OWL_E_TRVDD0P75",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/owl_e_trvdd0p75",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x04, "RAINBOW_OWL_W_TRVDD0P9",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/owl_w_trvdd0p9",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x05, "RAINBOW_OWL_E_TRVDD0P9",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/owl_e_trvdd0p9",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x06, "RAINBOW_MAX_N_VDD",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/max_n_vdd",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x07, "RAINBOW_MAX_M_VDD",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/max_m_vdd",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x08, "RAINBOW_MAX_S_VDD",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/max_s_vdd",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x09, "RAINBOW_HAMSA_VDD",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/hamsa_vdd",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x0A, "RAINBOW_OWL_W_VDD",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/owl_w_vdd",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x0B, "RAINBOW_OWL_E_VDD",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/owl_e_vdd",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x0C, "RAINBOW_MEDHA0_VDD",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/medha0_vdd",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x0D, "RAINBOW_MEDHA1_VDD",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/medha1_vdd",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x0E, "RAINBOW_VDDPHY_HBM1_HBM3_HBM5_HBM7",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/vddphy_hbm1_hbm3_hbm5_hbm7",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x0F, "RAINBOW_VPP_HBM1_HBM3_HBM5_HBM7",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/vpp_hbm1_hbm3_hbm5_hbm7",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x10, "RAINBOW_VDDQC_HBM1_HBM3_HBM5_HBM7",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/vddqc_hbm1_hbm3_hbm5_hbm7",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x11, "RAINBOW_VDDQL_HBM1_HBM3_HBM5_HBM7",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/vddql_hbm1_hbm3_hbm5_hbm7",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x12, "RAINBOW_VDDPHY_HBM0_HBM2_HBM4_HBM6",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/vddphy_hbm0_hbm2_hbm4_hbm6",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x13, "RAINBOW_VPP_HBM0_HBM2_HBM4_HBM6",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/vpp_hbm0_hbm2_hbm4_hbm6",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x14, "RAINBOW_VDDQC_HBM0_HBM2_HBM4_HBM6",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/vddqc_hbm0_hbm2_hbm4_hbm6",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x15, "RAINBOW_VDDQL_HBM0_HBM2_HBM4_HBM6",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/vddql_hbm0_hbm2_hbm4_hbm6",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x16, "RAINBOW_P1V5_W_RVDD",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/p1v5_w_rvdd",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x17, "RAINBOW_P1V5_E_RVDD",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/p1v5_e_rvdd",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x18, "RAINBOW_P0V9_OWL_W_PVDD",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/p0v9_owl_w_pvdd",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x19, "RAINBOW_P0V9_OWL_E_PVDD",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/p0v9_owl_e_pvdd",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x1A, "RAINBOW_PLL_VDDA15_HBM5_HBM7",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/pll_vdda15_hbm5_hbm7",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x1B, "RAINBOW_PLL_VDDA15_HBM1_HBM3",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/pll_vdda15_hbm1_hbm3",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x1C, "RAINBOW_PLL_VDDA15_HBM4_HBM6",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/pll_vdda15_hbm4_hbm6",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x1D, "RAINBOW_PLL_VDDA15_HBM0_HBM2",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/pll_vdda15_hbm0_hbm2",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x1E, "RAINBOW_PVDD1P5",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/pvdd1p5",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x1F, "RAINBOW_P1V5_PLL_VDDA_SOC",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/p1v5_pll_vdda_soc",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x20, "RAINBOW_P1V5_PLL_VDDA_OWL",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/p1v5_pll_vdda_owl",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x21, "RAINBOW_LDO_IN_1V2",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/ldo_in_1v2",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x22, "RAINBOW_P1V8",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/p1v8",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x23, "RAINBOW_P3V3",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/p3v3",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x24, "RAINBOW_P5V",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/p5v",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x25, "RAINBOW_P12V_UBC_PWRGD",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/p12v_ubc_pwrgd",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x26, "RAINBOW_P0V75_AVDD_HCSL ",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/p0v75_avdd_hcsl",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x27, "RAINBOW_P4V2",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/p4v2",
     "VoltageRegulatorFault"},
    {" RAINBOW FAULT", 0x50, "RAINBOW_POWER_ON_SEQUENCE_FAIL",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/power_on_sequence_fail",
     "PowerRailFault"},
    {" RAINBOW FAULT", 0x51, "RAINBOW_FM_ASIC_0_THERMTRIP_N",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/fm_asic_0_thermtrip_n",
     "DeviceOverOperatingTemperatureFault"},
    {" RAINBOW FAULT", 0x52, "RAINBOW_MEDHA1_HBM_CATTRIP_LS_LVC33_ALARM",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/medha1_hbm_cattrip_ls_lvc33_alarm",
     "DeviceOverOperatingTemperatureFault"},
    {" RAINBOW FAULT", 0x53, "RAINBOW_MEDHA0_HBM_CATTRIP_LS_LVC33_ALARM",
     "/xyz/openbmc_project/inventory/system/board/Santabarbara_Rainbow/medha0_hbm_cattrip_ls_lvc33_alarm",
     "DeviceOverOperatingTemperatureFault"},
};

static const std::map<EventType, const std::vector<FaultDesc>*> faultListMap = {
    {EventType::MTIA_FAULT, &mtiaFaultsList},
    {EventType::RAINBOW_FAULT, &rainbowFaultsList},
};

const std::vector<FaultDesc>* getFaultList(EventType eventType)
{
    if (auto it = faultListMap.find(eventType); it != faultListMap.end())
        return it->second;
    return nullptr;
}

const FaultDesc* lookupFaultByIndex(EventType eventType, uint8_t index)
{
    const auto* faultsList = getFaultList(eventType);
    if (!faultsList)
    {
        return nullptr;
    }

    auto it = std::find_if(
        faultsList->begin(), faultsList->end(),
        [index](const auto& fault) { return fault.index == index; });
    return it != faultsList->end() ? &(*it) : nullptr;
}

void recordEventLog(const FaultDesc& fault, EventAssert eventStatus,
                    const FaultData& data)
{
    if (fault.eventName == "PowerRailFault")
    {
        using namespace sdbusplus::error::xyz::openbmc_project::state::Power;
        using namespace sdbusplus::event::xyz::openbmc_project::state::Power;

        if (eventStatus == EventAssert::EVENT_ASSERTED)
        {
            lg2::commit(PowerRailFault(
                PowerRailFault::metadata_t<"POWER_RAIL">{"POWER_RAIL"},
                fault.object_path,
                PowerRailFault::metadata_t<"FAILURE_DATA">{"FAILURE_DATA"},
                std::format("{} - [{:02X} {:02X}]", fault.name, data.detail1,
                            data.detail2)));
        }
        else if (eventStatus == EventAssert::EVENT_DEASSERTED)
        {
            lg2::commit(PowerRailFaultRecovered(
                PowerRailFaultRecovered::metadata_t<"POWER_RAIL">{"POWER_RAIL"},
                fault.object_path));
        }
    }
    else if (fault.eventName == "VoltageRegulatorFault")
    {
        using namespace sdbusplus::error::xyz::openbmc_project::state::Power;
        using namespace sdbusplus::event::xyz::openbmc_project::state::Power;

        if (eventStatus == EventAssert::EVENT_ASSERTED)
        {
            lg2::commit(VoltageRegulatorFault(
                VoltageRegulatorFault::metadata_t<"VOLTAGE_REGULATOR">{
                    "VOLTAGE_REGULATOR"},
                fault.object_path,
                VoltageRegulatorFault::metadata_t<"FAILURE_DATA">{
                    "FAILURE_DATA"},
                std::format("{} - [{:02X} {:02X}]", fault.name, data.detail1,
                            data.detail2)));
        }
        else if (eventStatus == EventAssert::EVENT_DEASSERTED)
        {
            lg2::commit(VoltageRegulatorFaultRecovered(
                VoltageRegulatorFaultRecovered::metadata_t<"VOLTAGE_REGULATOR">{
                    "VOLTAGE_REGULATOR"},
                fault.object_path));
        }
    }
    else if (fault.eventName == "DeviceOverOperatingTemperatureFault")
    {
        using namespace sdbusplus::error::xyz::openbmc_project::state::Thermal;
        using namespace sdbusplus::event::xyz::openbmc_project::state::Thermal;

        if (eventStatus == EventAssert::EVENT_ASSERTED)
        {
            lg2::commit(DeviceOverOperatingTemperatureFault(
                DeviceOverOperatingTemperatureFault::metadata_t<"DEVICE">{
                    "DEVICE"},
                fault.object_path,
                DeviceOverOperatingTemperatureFault::metadata_t<"FAILURE_DATA">{
                    "FAILURE_DATA"},
                std::format("{} - [{:02X} {:02X}]", fault.name, data.detail1,
                            data.detail2)));
        }
        else if (eventStatus == EventAssert::EVENT_DEASSERTED)
        {
            lg2::commit(DeviceOperatingNormalTemperature(
                DeviceOperatingNormalTemperature::metadata_t<"DEVICE">{
                    "DEVICE"},
                fault.object_path));
        }
    }
}

int EventLogHandler::write(const message& eventData)
{
    if (eventData.size() != EVENT_LEN &&
        eventData.size() != EVENT_WITH_TIMESTAMP_LEN)
    {
        error("Invalid incoming data for event log, data size {SIZE} bytes",
              "SIZE", eventData.size());
        return PLDM_ERROR;
    }

    if (eventData[0] >= eventList.size())
    {
        error("Invalid event type for event log, event type {TYPE}", "TYPE",
              eventData[0]);
        return PLDM_ERROR;
    }

    if (eventData.size() == EVENT_WITH_TIMESTAMP_LEN)
    {
        // Check whether the event is a retry event based on the timestamp
        // recorded by the BIC. This works as long as each event type is sent by
        // only one thread on the BIC, i.e., no race condition occurs for the
        // same event type.
        uint32_t eventTimeStamp = (eventData[5] << 12) + (eventData[6] << 8) +
                                  (eventData[7] << 4) + eventData[8];
        std::pair<pldm_tid_t, EventType> eventKey =
            std::make_pair(tid, static_cast<EventType>(eventData[0]));
        if (receivedEventTimeStamp.count(eventKey) &&
            receivedEventTimeStamp[eventKey] == eventTimeStamp)
        {
            lg2::info(
                "oem event: TID {TID}: Event time stamp {EVTT} skip retry event",
                "TID", tid, "EVTT", eventTimeStamp);
            return PLDM_SUCCESS;
        }
        receivedEventTimeStamp[eventKey] = eventTimeStamp;
    }

    std::string slotNum = pldm::oem_meta::getSlotNumberStringByTID(tid);
    const std::string& event = handleEventType(eventData, slotNum);
    addSystemEventLogAndJournal(eventData, event, slotNum);

    return PLDM_SUCCESS;
}

void checkEventAssert(const message& eventData, std::string& event,
                      const std::string& slotNum)
{
    EventAssert eventStatus = static_cast<EventAssert>(eventData[1]);
    switch (eventStatus)
    {
        case EventAssert::EVENT_ASSERTED:
            event.insert(0, "Event: Host" + slotNum);
            event.append(", ASSERTED");
            break;
        case EventAssert::EVENT_DEASSERTED:
            event.insert(0, "Event: Host" + slotNum);
            event.append(", DEASSERTED");
            break;
    }
}

const std::string EventLogHandler::handleEventType(
    const message& eventData, const std::string& slotNum) const
{
    std::string event;
    EventType eventType = static_cast<EventType>(eventData[0]);
    switch (eventType)
    {
        case EventType::PMALERT_ASSERTION:
        case EventType::VR_FAULT:
        {
            if (eventData[2] >= vr_source.size())
            {
                error("Invalid vr source {VR_SOURCE}", "VR_SOURCE",
                      eventData[2]);
                return {};
            }

            if (vr_source[eventData[2]].isPMBus)
            {
                event = eventList[static_cast<int>(eventType)] +
                        std::string(" :") + vr_source[eventData[2]].netName +
                        " status: 0x" + std::format("{:02x}", eventData[3]) +
                        std::format("{:02x}", eventData[4]);
            }
            else
            {
                event = eventList[static_cast<int>(eventType)] +
                        std::string(" :") + vr_source[eventData[2]].netName;
            }
            break;
        }
        case EventType::DIMM_PMIC_ERROR:
        {
            if ((eventData[2] >= dimm_id.size()) ||
                (eventData[3] >= pmic_event_type.size()))
            {
                error(" Invalid dimm id {ID}, pmic event type {TYPE}", "ID",
                      eventData[2], "TYPE", eventData[3]);
                return {};
            }

            event = eventList[static_cast<int>(eventType)] + std::string(" :") +
                    dimm_id[eventData[2]] + pmic_event_type[eventData[3]];
            break;
        }
        case EventType::CPU_THERMAL_TRIP:
        case EventType::HSC_OCP:
        case EventType::P12V_STBY_UV:
        case EventType::FAST_PROCHOT_ASSERTION:
        case EventType::FRB3_TIMER_EXPIRED:
        case EventType::POWER_ON_SEQUENCE_FAILURE:
        case EventType::ADDC_DUMP:
        case EventType::BMC_COMES_OUT_OF_COLD_RESET:
        case EventType::BIOS_FRB12_WATCHDOG_TIMER_EXPIRED:
        case EventType::BIC_POWER_FAILURE:
        case EventType::CPU_POWER_FAILURE:
        case EventType::V_BOOT_FAILURE:
        case EventType::BMC_IS_REQUESTED_TO_REBOOT:
        case EventType::CHASSIS_POWER_ON_IS_INITIATED_BY_NIC_INSERTION:
        case EventType::BLADE_IS_POWERED_CYCLE_BY_BLADE_BUTTON:
        case EventType::CHASSIS_IS_POWERED_CYCLE_BY_SLED_BUTTON:
        case EventType::HSC_FAULT:
        case EventType::SYS_THROTTLE:
        case EventType::SYSTEM_MANAGEMENT_ERROR:
        case EventType::POST_COMPLETED:
        case EventType::FAN_ERROR:
        case EventType::HDT_PRSNT_ASSERTION:
        case EventType::PLTRST_ASSERTION:
        case EventType::APML_ALERT_ASSERTION:
        case EventType::CXL1_HB:
        case EventType::CXL2_HB:
        case EventType::POST_STARTED:
        case EventType::POST_ENDED:
        case EventType::
            PROCHOT_IS_TRIGGERED_DUE_TO_SD_SENSOR_READING_EXCEED_UCR:
        case EventType::FRB2_WDT_HARD_RST:
        case EventType::FRB2_WDT_PWR_DOWN:
        case EventType::FRB2_WDT_PWR_CYCLE:
        case EventType::OS_LOAD_WDT_EXPIRED:
        case EventType::OS_LOAD_WDT_HARD_RST:
        case EventType::OS_LOAD_WDT_PWR_DOWN:
        case EventType::OS_LOAD_WDT_PWR_CYCLE:
        case EventType::MTIA_FAULT:
        case EventType::POST_TIMEOUTED:
        case EventType::RAINBOW_FAULT:
        {
            event = eventList[static_cast<int>(eventType)];
            break;
        }
        default:
        {
            event = "Event type not supported";
            break;
        }
    }
    checkEventAssert(eventData, event, slotNum);

    return event;
}

void EventLogHandler::addSystemEventLogAndJournal(
    const message& eventData, const std::string& event,
    const std::string& slotNum) const
{
    EventType eventType = static_cast<EventType>(eventData[0]);
    switch (eventType)
    {
        case EventType::CPU_THERMAL_TRIP:
        case EventType::FAST_PROCHOT_ASSERTION:
        {
            using namespace sdbusplus::error::xyz::openbmc_project::state::
                Thermal;

            lg2::commit(DeviceOverOperatingTemperatureFault(
                DeviceOverOperatingTemperatureFault::metadata_t<"DEVICE">{
                    "DEVICE"},
                sdbusplus::message::object_path(),
                DeviceOverOperatingTemperatureFault::metadata_t<"FAILURE_DATA">{
                    "FAILURE_DATA"},
                event));
            break;
        }
        case EventType::HSC_OCP:
        case EventType::P12V_STBY_UV:
        case EventType::POWER_ON_SEQUENCE_FAILURE:
        case EventType::DIMM_PMIC_ERROR:
        {
            using namespace sdbusplus::error::xyz::openbmc_project::state::
                Power;

            lg2::commit(PowerRailFault(
                PowerRailFault::metadata_t<"POWER_RAIL">{"POWER_RAIL"},
                sdbusplus::message::object_path(),
                PowerRailFault::metadata_t<"FAILURE_DATA">{"FAILURE_DATA"},
                event));
            break;
        }
        case EventType::CXL1_HB:
        case EventType::CXL2_HB:
        case EventType::PLTRST_ASSERTION:
        case EventType::POST_STARTED:
        case EventType::POST_ENDED:
        {
            break;
        }
        case EventType::
            PROCHOT_IS_TRIGGERED_DUE_TO_SD_SENSOR_READING_EXCEED_UCR:
        {
            std::string device =
                "/xyz/openbmc_project/State/Thermal/host" + slotNum + "/cpu0";
            auto oemData =
                "0x" + std::format("{:02x}{:02x}", eventData[3], eventData[4]);
            std::string failureData =
                "SD sensor reading exceed UCR, data: " + oemData;

            if (oemData == "0x0000")
            {
                using namespace sdbusplus::event::xyz::openbmc_project::state::
                    Thermal;

                lg2::commit(DeviceOperatingNormalTemperature(
                    DeviceOperatingNormalTemperature::metadata_t<"DEVICE">{
                        "DEVICE"},
                    sdbusplus::message::object_path(device)));
            }
            else
            {
                using namespace sdbusplus::error::xyz::openbmc_project::state::
                    Thermal;

                lg2::commit(DeviceOverOperatingTemperatureFault(
                    DeviceOverOperatingTemperatureFault::metadata_t<"DEVICE">{
                        "DEVICE"},
                    sdbusplus::message::object_path(device),
                    DeviceOverOperatingTemperatureFault::metadata_t<
                        "FAILURE_DATA">{"FAILURE_DATA"},
                    failureData));
            }
            break;
        }
        case EventType::MTIA_FAULT:
        {
            EventAssert eventStatus = static_cast<EventAssert>(eventData[1]);
            const auto index = eventData[2];
            const auto* faultEvent = lookupFaultByIndex(eventType, index);
            if (!faultEvent)
            {
                error("Invalid MTIA fault source 0x{SOURCE:X}", "SOURCE",
                      static_cast<int>(eventData[2]));
                break;
            }

            FaultData data{eventData[3], eventData[4]};
            recordEventLog(*faultEvent, eventStatus, data);
            break;
        }
        case EventType::RAINBOW_FAULT:
        {
            EventAssert eventStatus = static_cast<EventAssert>(eventData[1]);
            const auto index = eventData[2];

            const auto* faultEvent = lookupFaultByIndex(eventType, index);
            if (!faultEvent)
            {
                error("Invalid Rainbow fault source 0x{SOURCE:X}", "SOURCE",
                    static_cast<int>(index));
                break;
            }

            int slot = -1;
            switch (tid)
            {
                case 10: slot = 0; break;
                case 20: slot = 1; break;
                case 30: slot = 2; break;
                case 40: slot = 3; break;
                default:
                    error("Invalid TID {TID} for rainbow slot mapping", "TID", tid);
                    break;
            }

            FaultDesc updatedFault = *faultEvent;

            if (slot != -1)
            {
                constexpr std::string_view needle = "/Santabarbara_Rainbow";

                std::string newPath = updatedFault.object_path;
                size_t pos = newPath.find(needle);

                if (pos != std::string::npos)
                {
                    newPath.insert(pos + needle.size(), "_" + std::to_string(slot));
                }
                else
                {
                    newPath += "_" + std::to_string(slot);
                }

                updatedFault.object_path = std::move(newPath);
            }

            FaultData data{eventData[3], eventData[4]};
            recordEventLog(updatedFault, eventStatus, data);
            break;
        }
        case EventType::PMALERT_ASSERTION:
        case EventType::FRB3_TIMER_EXPIRED:
        case EventType::ADDC_DUMP:
        case EventType::BMC_COMES_OUT_OF_COLD_RESET:
        case EventType::BIOS_FRB12_WATCHDOG_TIMER_EXPIRED:
        case EventType::BIC_POWER_FAILURE:
        case EventType::CPU_POWER_FAILURE:
        case EventType::V_BOOT_FAILURE:
        case EventType::BMC_IS_REQUESTED_TO_REBOOT:
        case EventType::CHASSIS_POWER_ON_IS_INITIATED_BY_NIC_INSERTION:
        case EventType::BLADE_IS_POWERED_CYCLE_BY_BLADE_BUTTON:
        case EventType::CHASSIS_IS_POWERED_CYCLE_BY_SLED_BUTTON:
        case EventType::HSC_FAULT:
        case EventType::SYS_THROTTLE:
        case EventType::VR_FAULT:
        case EventType::SYSTEM_MANAGEMENT_ERROR:
        case EventType::POST_COMPLETED:
        case EventType::FAN_ERROR:
        case EventType::HDT_PRSNT_ASSERTION:
        case EventType::APML_ALERT_ASSERTION:
        case EventType::FRB2_WDT_HARD_RST:
        case EventType::FRB2_WDT_PWR_DOWN:
        case EventType::FRB2_WDT_PWR_CYCLE:
        case EventType::OS_LOAD_WDT_EXPIRED:
        case EventType::OS_LOAD_WDT_HARD_RST:
        case EventType::OS_LOAD_WDT_PWR_DOWN:
        case EventType::OS_LOAD_WDT_PWR_CYCLE:
        case EventType::POST_TIMEOUTED:
        {
            pldm::utils::reportError(event.c_str()); // Create log on Dbus
            break;
        }
        default:
        {
            lg2::error("{ERROR}", "ERROR", event); // Create log in journal
        }
    }
}

} // namespace pldm::responder::oem_meta
