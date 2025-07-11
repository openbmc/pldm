
#pragma once

namespace pldm::oem_meta
{
    enum class RecordType : uint8_t
    {
        SYSTEM_EVENT_RECORD = 0x02,
        UNIFIED_BIOS_SEL = 0xFB,
    };

    enum class UnifiedError : uint8_t
    {
        UNIFIED_PCIE_ERR = 0x0,
        UNIFIED_MEM_ERR = 0x1,
        UNIFIED_UPI_ERR = 0x2,
        UNIFIED_IIO_ERR = 0x3,
        UNIFIED_MCA_ERR = 0x4,
        UNIFIED_MCA_ERR_EXT = 0x5,
        UNIFIED_RP_PIO_1st = 0x6,
        UNIFIED_RP_PIO_2nd = 0x7,
        UNIFIED_POST_EVENT = 0x8,
        UNIFIED_PCIE_EVENT = 0x9,
        UNIFIED_MEM_EVENT = 0xA,
        UNIFIED_UPI_EVENT = 0xB,
        UNIFIED_BOOT_GUARD = 0xC,
        UNIFIED_PPR_EVENT = 0xD,
        UNIFIED_CXL_MEM_ERR = 0xE,
    };

    enum class MemoryError : uint8_t
    {
        MEMORY_TRAINING_ERR = 0x0,
        MEMORY_CORRECTABLE_ERR = 0x1,
        MEMORY_UNCORRECTABLE_ERR = 0x2,
        MEMORY_CORR_ERR_PTRL_SCR = 0x3,
        MEMORY_UNCORR_ERR_PTRL_SCR = 0x4,
        MEMORY_PARITY_ERR_PCC0 = 0x5,
        MEMORY_PARITY_ERR_PCC1 = 0x6,
        MEMORY_PMIC_ERR = 0x7,
        MEMORY_CXL_TRAINNING_ERR = 0x8,
        MEMORY_CXL_MEM_SPD_CRC_ERR = 0x9,
        MEMORY_CXL_MEM_SPD_NULL_DATA_ERR = 0xA,
        MEMORY_CXL_MEM_INIT_TIMEOUT_ERR = 0xB,
    };

    enum class PostError : uint8_t
    {
        POST_PXE_BOOT_FAIL = 0x0,
        POST_CMOS_CLEARED = 0x1,
        POST_TPM_SELF_TEST_FAIL = 0x2,
        POST_BOOT_DRIVE_FAIL = 0x3,
        POST_DATA_DRIVE_FAIL = 0x4,
        POST_INVALID_BOOT_ORDER = 0x5,
        POST_HTTP_BOOT_FAIL = 0x6,
        POST_GET_CERT_FAIL = 0x7,
        POST_PASSWD_CLEARED_BY_JUMPER = 0x8,
        POST_DXE_FV_CHK_FAIL = 0x9,
        POST_AMD_ABL_FAIL = 0xA,
        POST_DRAM_PAGE_RETIRED = 0xB,
        POST_DRAM_CHANNEL_RETIRED = 0xC,
        POST_CXL_NOT_READY = 0xD,
        POST_CXL_ERR_RECORD_CLEARED_BY_BIOS = 0xE,
    };

    enum class PcieEvent : uint8_t
    {
        PCIE_DPC = 0x0,
    };

    enum class MemoryEvent : uint8_t
    {
        MEM_PPR = 0x0,
        MEM_ADDDC = 0x5,
        MEM_NO_DIMM = 0x7,
        MEM_CXL_POST_PPR = 0x8,
    };

    enum class UpiError : uint8_t
    {
        UPI_INIT_ERR = 0x0,
    };

    static constexpr auto errorSeverityDetail = std::to_array(
        {"Correctable Error", "Deferred Error", "Uncorrected Recoverable Error",
         "Uncorrected Thread Fatal Error", "Uncorrected System Fatal Error"});

    static constexpr auto machineCheckBank = std::to_array(
        {"LS",
         "IF",
         "L2",
         "DE",
         "RAZ",
         "EX",
         "FP",
         "L3",
         "L3",
         "L3",
         "L3",
         "L3",
         "L3",
         "L3",
         "L3",
         "MP5",
         "PCS_GMI",
         "PCS_GMI",
         "KPX_GMI",
         "KPX_GMI",
         "MPDMA",
         "UMC",
         "UMC",
         "CS/RAZ",
         "CS/RAZ",
         "PCIE",
         "SATA/USB/PCIE",
         "NBIF",
         "PIE/PSP/KPX_WALF/NBIF/USB/RAZ",
         "SMU/SHUB",
         "PIE/PSP/PCS_XGMI",
         "KPX_SERDES/RAZ"});

    static constexpr auto memoryError = std::to_array(
        {"Memory training failure", "Memory correctable error",
         "Memory uncorrectable error",
         "Memory correctable error (Patrol scrub)",
         "Memory uncorrectable error (Patrol scrub)",
         "Memory Parity Error (PCC=0)", "Memory Parity Error (PCC=1)",
         "Memory PMIC Error", "CXL Memory training error",
         "CXL Memory SPD CRC error", "CXL Memory SPD NULL DATA error",
         "CXL Memory initialized timeout error", "Reserved"});

    static constexpr auto certEvent =
        std::to_array({"No certificate at BMC", "IPMI transaction fail",
                       "Certificate data corrupted", "Reserved"});

    static constexpr auto postError = std::to_array(
        {"System PXE boot fail", "CMOS/NVRAM configuration cleared",
         "TPM Self-Test Fail", "Boot Drive failure", "Data Drive failure",
         "Received invalid boot order request from BMC",
         "System HTTP boot fail", "BIOS fails to get the certificate from BMC",
         "Password cleared by jumper", "DXE FV check failure",
         "AMD ABL failure", "DRAM Page Retired", "DRAM Channel Retired",
         "CXL not ready", "CXL error record cleared by BIOS", "Reserved"});

    static constexpr auto cxlError = std::to_array(
        {"CXL_ELOG_IO", "CXL_ELOG_CACHE_MEM", "CXL_ELOG_COMPONENT_EVENT",
         "Reserved"});

    static constexpr auto pcieEvent = std::to_array(
        {"PCIe DPC Event", "PCIe LER Event",
         "PCIe Link Retraining and Recovery",
         "PCIe Link CRC Error Check and Retry", "PCIe Corrupt Data Containment",
         "PCIe Express ECRC", "Reserved"});

    static constexpr auto memoryEvent = std::to_array(
        {"Memory PPR event", "Memory Correctable Error logging limit reached",
         "Memory disable/map-out for FRB", "Memory SDDC",
         "Memory Address range/Partial mirroring", "Memory ADDDC",
         "Memory SMBus hang recovery", "No DIMM in System",
         "CXL POST PPR event", "Reserved"});

    static constexpr auto memoryPprRepairTime =
        std::to_array({"Boot time", "Autonomous", "Run time", "Reserved"});

    static constexpr auto memoryPprEvent =
        std::to_array({"PPR success", "PPR fail", "PPR request", "Reserved"});

    static constexpr auto memoryCXLPostPprEvent =
        std::to_array({"PPR success", "PPR full"});

    static constexpr auto memoryAdddcEvent = std::to_array(
        {"Bank VLS", "r-Bank VLS + re-buddy", "r-Bank VLS + Rank VLS",
         "r-Rank VLS + re-buddy"});

    static constexpr auto upiEvent = std::to_array(
        {"Successful LLR without Phy Reinit", "Successful LLR with Phy Reinit",
         "COR Phy Lane failure, recovery in x8 width", "Reserved"});

    static constexpr auto pprEvent =
        std::to_array({"PPR disable", "Soft PPR", "Hard PPR"});

    static constexpr auto upiError =
        std::to_array({"UPI Init error", "Reserved"});

    static constexpr auto memoryCxlEvent =
        std::to_array({"DIMM A0", "DIMM B0", "DIMM A1", "DIMM B1", "Unknown"});


	struct DimmInfo
	{
			uint8_t sled;
			uint8_t socket;
			uint8_t channel;
			uint8_t slot;
	};
} // namespace pldm::oem_meta
