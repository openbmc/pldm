#pragma once

#include "libcper/Cper.h"

#include <unistd.h>

#include <cstdint>
#include <cstdio>

namespace pldm
{
namespace oem_ampere
{
constexpr auto logBusName = "xyz.openbmc_project.Logging.IPMI";
constexpr auto logPath = "/xyz/openbmc_project/Logging/IPMI";
constexpr auto logIntf = "xyz.openbmc_project.Logging.IPMI";
constexpr uint8_t SENSOR_TYPE_OEM = 0xF0;

/* Memory definitions */
constexpr uint8_t MEM_ERROR_TYPE_PARITY = 8;
constexpr uint16_t ERROR_TYPE_ID_MCU = 1;
constexpr uint16_t SUBTYPE_ID_PARITY = 9;

/* PCIe definitions */
constexpr uint16_t ERROR_SUBTYPE_PCIE_AER_ROOT_PORT = 0;
constexpr uint16_t ERROR_SUBTYPE_PCIE_AER_DEVICE = 1;
constexpr uint64_t CPER_PCIE_VALID_PORT_TYPE = 0x0001;
constexpr uint32_t CPER_PCIE_PORT_TYPE_ROOT_PORT = 4;

typedef struct
{
    uint8_t formatVersion;
    uint8_t formatType;
    uint16_t length;
} __attribute__((packed)) CommonEventData;

void decodeCperRecord(const uint8_t* data, size_t eventDataSize,
                      EFI_AMPERE_ERROR_DATA* ampSpecHdr);
void addCperSELLog(uint8_t TID, uint16_t eventID, EFI_AMPERE_ERROR_DATA* p);

} // namespace oem_ampere
} // namespace pldm
