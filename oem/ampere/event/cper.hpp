#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <fstream>
#include <vector>

namespace pldm
{
namespace oem
{

#define CPER_TEMP_DIR "/usr/share/pldm/cper/"
#define CPER_LOG_PATH "/var/lib/faultlogs/cper/"
#define SENSOR_TYPE_OEM 0xF0

typedef struct
{
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
} __attribute__((packed)) Guid;

typedef struct
{
    uint8_t Seconds;
    uint8_t Minutes;
    uint8_t Hours;
    uint8_t Flag;
    uint8_t Day;
    uint8_t Month;
    uint8_t Year;
    uint8_t Century;
} Timestamp;

/*----------------------------------------------------------------------------*/
/*                               Ampere Header                                */
/*----------------------------------------------------------------------------*/

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
} __attribute__((packed)) AmpereSpecData;

/*----------------------------------------------------------------------------*/
/*                               CPER Common Header                           */
/*----------------------------------------------------------------------------*/

/* Record Header, UEFI v2.9 sec N.2.1 */
typedef struct
{
    uint32_t SignatureStart;
    uint16_t Revision;
    uint32_t SignatureEnd;
    uint16_t SectionCount;
    uint32_t ErrorSeverity;
    uint32_t ValidationBits;
    uint32_t RecordLength;
    Timestamp TimeStamp;
    Guid PlatformID;
    Guid PartitionID;
    Guid CreatorID;
    Guid NotificationType;
    uint64_t RecordID;
    uint32_t Flags;
    uint64_t PersistenceInfo;
    uint8_t Resv1[12];
} __attribute__((packed)) CPERRecodHeader;

/* Section Descriptor, UEFI v2.9 sec N.2.2 */
typedef struct
{
    uint32_t SectionOffset;
    uint32_t SectionLength;
    uint16_t Revision;
    uint8_t SecValidMask;
    uint8_t Resv1;
    uint32_t SectionFlags;
    Guid SectionType;
    Guid FruId;
    uint32_t Severity;
    char FruString[20];
} __attribute__((packed)) CPERSectionDescriptor;

/*----------------------------------------------------------------------------*/
/*                               Processor ARM Header                         */
/*----------------------------------------------------------------------------*/

/* ARM Processor Error Section */
typedef struct
{
    uint32_t ValidFields;
    uint16_t ErrInfoNum;
    uint16_t ContextInfoNum;
    uint32_t SectionLength;
    uint32_t ErrorAffinityLevel;
    uint64_t MPIDR_EL1;
    uint64_t MIDR_EL1;
    uint32_t RunningState;
    uint32_t PsciState;
} __attribute__((packed)) CPERSecProcArm;

/* ARM Processor Error Information Structure */
typedef struct
{
    uint8_t Version;
    uint8_t Length;
    uint16_t ValidationBits;
    uint8_t Type;
    uint16_t MultipleError;
    uint8_t Flags;
    uint64_t ErrorInformation; /*TBD*/
    uint64_t VirtualFaultAddress;
    uint64_t PhysicalFaultAddress;
} __attribute__((packed)) CPERArmErrInfo;

/* ARM Processor Context Information Structure */
typedef struct
{
    uint16_t Version;
    uint16_t RegisterContextType;
    uint32_t RegisterArraySize;
} __attribute__((packed)) CPERArmCtxInfo;

///
/// ARM Processor Context Register Types
///
#define ARM_CONTEXT_TYPE_AARCH32_GPR 0
#define ARM_CONTEXT_TYPE_AARCH32_EL1 1
#define ARM_CONTEXT_TYPE_AARCH32_EL2 2
#define ARM_CONTEXT_TYPE_AARCH32_SECURE 3
#define ARM_CONTEXT_TYPE_AARCH64_GPR 4
#define ARM_CONTEXT_TYPE_AARCH64_EL1 5
#define ARM_CONTEXT_TYPE_AARCH64_EL2 6
#define ARM_CONTEXT_TYPE_AARCH64_EL3 7
#define ARM_CONTEXT_TYPE_MISC 8

typedef struct
{
    uint32_t R0;
    uint32_t R1;
    uint32_t R2;
    uint32_t R3;
    uint32_t R4;
    uint32_t R5;
    uint32_t R6;
    uint32_t R7;
    uint32_t R8;
    uint32_t R9;
    uint32_t R10;
    uint32_t R11;
    uint32_t R12;
    uint32_t R13_sp;
    uint32_t R14_lr;
    uint32_t R15_pc;
} __attribute__((packed)) ARM_V8_AARCH32_GPR;

typedef struct
{
    uint32_t Dfar;
    uint32_t Dfsr;
    uint32_t Ifar;
    uint32_t Isr;
    uint32_t Mair0;
    uint32_t Mair1;
    uint32_t Midr;
    uint32_t Mpidr;
    uint32_t Nmrr;
    uint32_t Prrr;
    uint32_t Sctlr_Ns;
    uint32_t Spsr;
    uint32_t Spsr_Abt;
    uint32_t Spsr_Fiq;
    uint32_t Spsr_Irq;
    uint32_t Spsr_Svc;
    uint32_t Spsr_Und;
    uint32_t Tpidrprw;
    uint32_t Tpidruro;
    uint32_t Tpidrurw;
    uint32_t Ttbcr;
    uint32_t Ttbr0;
    uint32_t Ttbr1;
    uint32_t Dacr;
} __attribute__((packed)) ARM_AARCH32_EL1_CONTEXT_REGISTERS;

typedef struct
{
    uint32_t Elr_Hyp;
    uint32_t Hamair0;
    uint32_t Hamair1;
    uint32_t Hcr;
    uint32_t Hcr2;
    uint32_t Hdfar;
    uint32_t Hifar;
    uint32_t Hpfar;
    uint32_t Hsr;
    uint32_t Htcr;
    uint32_t Htpidr;
    uint32_t Httbr;
    uint32_t Spsr_Hyp;
    uint32_t Vtcr;
    uint32_t Vttbr;
    uint32_t Dacr32_El2;
} __attribute__((packed)) ARM_AARCH32_EL2_CONTEXT_REGISTERS;

typedef struct
{
    uint32_t Sctlr_S;
    uint32_t Spsr_Mon;
} __attribute__((packed)) ARM_AARCH32_SECURE_CONTEXT_REGISTERS;

typedef struct
{
    uint64_t X0;
    uint64_t X1;
    uint64_t X2;
    uint64_t X3;
    uint64_t X4;
    uint64_t X5;
    uint64_t X6;
    uint64_t X7;
    uint64_t X8;
    uint64_t X9;
    uint64_t X10;
    uint64_t X11;
    uint64_t X12;
    uint64_t X13;
    uint64_t X14;
    uint64_t X15;
    uint64_t X16;
    uint64_t X17;
    uint64_t X18;
    uint64_t X19;
    uint64_t X20;
    uint64_t X21;
    uint64_t X22;
    uint64_t X23;
    uint64_t X24;
    uint64_t X25;
    uint64_t X26;
    uint64_t X27;
    uint64_t X28;
    uint64_t X29;
    uint64_t X30;
    uint64_t Sp;
} __attribute__((packed)) ARM_V8_AARCH64_GPR;

typedef struct
{
    uint64_t Elr_El1;
    uint64_t Esr_El1;
    uint64_t Far_El1;
    uint64_t Isr_El1;
    uint64_t Mair_El1;
    uint64_t Midr_El1;
    uint64_t Mpidr_El1;
    uint64_t Sctlr_El1;
    uint64_t Sp_El0;
    uint64_t Sp_El1;
    uint64_t Spsr_El1;
    uint64_t Tcr_El1;
    uint64_t Tpidr_El0;
    uint64_t Tpidr_El1;
    uint64_t Tpidrro_El0;
    uint64_t Ttbr0_El1;
    uint64_t Ttbr1_El1;
} __attribute__((packed)) ARM_AARCH64_EL1_CONTEXT_REGISTERS;

typedef struct
{
    uint64_t Elr_El2;
    uint64_t Esr_El2;
    uint64_t Far_El2;
    uint64_t Hacr_El2;
    uint64_t Hcr_El2;
    uint64_t Hpfar_El2;
    uint64_t Mair_El2;
    uint64_t Sctlr_El2;
    uint64_t Sp_El2;
    uint64_t Spsr_El2;
    uint64_t Tcr_El2;
    uint64_t Tpidr_El2;
    uint64_t Ttbr0_El2;
    uint64_t Vtcr_El2;
    uint64_t Vttbr_El2;
} __attribute__((packed)) ARM_AARCH64_EL2_CONTEXT_REGISTERS;

typedef struct
{
    uint64_t Elr_El3;
    uint64_t Esr_El3;
    uint64_t Far_El3;
    uint64_t Mair_El3;
    uint64_t Sctlr_El3;
    uint64_t Sp_El3;
    uint64_t Spsr_El3;
    uint64_t Tcr_El3;
    uint64_t Tpidr_El3;
    uint64_t Ttbr0_El3;
} __attribute__((packed)) ARM_AARCH64_EL3_CONTEXT_REGISTERS;

typedef struct
{
    uint64_t MrsOp2:3;
    uint64_t MrsCrm:4;
    uint64_t MrsCrn:4;
    uint64_t MrsOp1:3;
    uint64_t MrsO0:1;
    uint64_t Value:64;
} __attribute__((packed)) ARM_MISC_CONTEXT_REGISTER;

/*----------------------------------------------------------------------------*/
/*                               Platform Memory Header                       */
/*----------------------------------------------------------------------------*/

/* Memory Error Section */
typedef struct
{
    uint64_t ValidFields;
    uint64_t ErrorStatus;
    uint64_t PhysicalAddress;     // Error physical address
    uint64_t PhysicalAddressMask; // Grnaularity
    uint16_t Node;                // Node #
    uint16_t Card;
    uint16_t ModuleRank;          // Module or Rank#
    uint16_t Bank;
    uint16_t Device;
    uint16_t Row;
    uint16_t Column;
    uint16_t BitPosition;
    uint64_t RequestorId;
    uint64_t ResponderId;
    uint64_t TargetId;
    uint8_t ErrorType;
    uint8_t Extended;
    uint16_t RankNum;
    uint16_t CardHandle;
    uint16_t ModuleHandle;
} __attribute__((packed)) CPERSecMemErr;

#define MEM_ERROR_TYPE_PARITY 8
#define ERROR_TYPE_ID_MCU 1
#define SUBTYPE_ID_PARITY 9

/*----------------------------------------------------------------------------*/
/*                               PCIE Header                                  */
/*----------------------------------------------------------------------------*/

///
/// PCI Slot number
///
typedef struct
{
    uint16_t Resv1:3;
    uint16_t Number:13;
} __attribute__((packed)) GENERIC_ERROR_PCI_SLOT;

///
/// PCIe Root Port PCI/bridge PCI compatible device number and
/// bus number information to uniquely identify the root port or
/// bridge. Default values for both the bus numbers is zero.
///
typedef struct
{
    uint16_t VendorId;
    uint16_t DeviceId;
    uint8_t ClassCode[3];
    uint8_t Function;
    uint8_t Device;
    uint16_t Segment;
    uint8_t PrimaryOrDeviceBus;
    uint8_t SecondaryBus;
    GENERIC_ERROR_PCI_SLOT Slot;
    uint8_t Resv1;
} __attribute__((packed)) GENERIC_ERROR_PCIE_DEV_BRIDGE_ID;

///
/// PCIe Capability Structure
///
typedef struct
{
    uint8_t PcieCap[60];
} __attribute__((packed)) PCIE_ERROR_DATA_CAPABILITY;

///
/// PCIe Advanced Error Reporting Extended Capability Structure.
///
typedef struct
{
    uint8_t PcieAer[96];
} __attribute__((packed)) PCIE_ERROR_DATA_AER;

typedef struct
{
    uint64_t ValidFields;
    uint32_t PortType;
    uint32_t Version;
    uint32_t CommandStatus;
    uint32_t Resv2;
    GENERIC_ERROR_PCIE_DEV_BRIDGE_ID DevBridge;
    uint64_t SerialNo;
    uint32_t BridgeControlStatus;
    PCIE_ERROR_DATA_CAPABILITY Capability;
    PCIE_ERROR_DATA_AER AerInfo;
} __attribute__((packed)) CPERSecPcieErr;

#define ERROR_TYPE_PCIE_AER 7
#define ERROR_SUBTYPE_PCIE_AER_ROOT_PORT 0
#define ERROR_SUBTYPE_PCIE_AER_DEVICE 1
#define CPER_PCIE_VALID_PORT_TYPE 0x0001
#define CPER_PCIE_PORT_TYPE_ROOT_PORT 4

/*----------------------------------------------------------------------------*/
/*                               Common Header                                */
/*----------------------------------------------------------------------------*/

void decodeCperRecord(const uint8_t* data, long pos, AmpereSpecData* ampSpecHdr,
                      std::ofstream& out);
void addCperSELLog(uint8_t TID, uint16_t eventID, AmpereSpecData* p);

} // namespace oem
} // namespace pldm
