#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <fstream>
#include <vector>

namespace pldm
{
namespace oem_ampere
{
constexpr auto logBusName = "xyz.openbmc_project.Logging.IPMI";
constexpr auto logPath = "/xyz/openbmc_project/Logging/IPMI";
constexpr auto logIntf = "xyz.openbmc_project.Logging.IPMI";
#define SENSOR_TYPE_OEM 0xF0

typedef struct
{
    uint8_t formatVersion;
    uint8_t formatType;
    uint16_t length;
} __attribute__((packed)) CommonEventData;

union AmpereSpecDataType
{
    struct AmpereSpecDataTypeStruct
    {
        uint16_t ipType:11;
        uint16_t isBert:1;
        uint16_t payloadType:4;
    } member;

    uint16_t type;
} __attribute__((packed));

typedef struct
{
    union AmpereSpecDataType typeId;
    uint16_t subTypeId;
    uint32_t instanceId;
} __attribute__((packed)) EFI_AMPERE_ERROR_DATA;

/*----------------------------------------------------------------------------*/
/*                               Platform Memory Header                       */
/*----------------------------------------------------------------------------*/

#define MEM_ERROR_TYPE_PARITY 8
#define ERROR_TYPE_ID_MCU 1
#define SUBTYPE_ID_PARITY 9

/*----------------------------------------------------------------------------*/
/*                               PCIE Header                                  */
/*----------------------------------------------------------------------------*/

#define ERROR_TYPE_PCIE_AER 7
#define ERROR_SUBTYPE_PCIE_AER_ROOT_PORT 0
#define ERROR_SUBTYPE_PCIE_AER_DEVICE 1
#define CPER_PCIE_VALID_PORT_TYPE 0x0001
#define CPER_PCIE_PORT_TYPE_ROOT_PORT 4

/*----------------------------------------------------------------------------*/
/*                               Common Header                                */
/*----------------------------------------------------------------------------*/

void decodeCperRecord(const uint8_t* data, size_t eventDataSize,
                      pldm::oem_ampere::EFI_AMPERE_ERROR_DATA* ampSpecHdr);
void addCperSELLog(uint8_t TID, uint16_t eventID,
                   pldm::oem_ampere::EFI_AMPERE_ERROR_DATA* p);

} // namespace oem_ampere
} // namespace pldm
