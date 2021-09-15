#ifndef LIBPLDM_FW_UPDATE_HPP
#define LIBPLDM_FW_UPDATE_HPP

#include "libncsi/ncsi.h"
#include "libpldm/base.h"
#include "utils.h"

#include <stddef.h>
#include <stdint.h>

#define PDLM_FW_MAX_DESCRIPTOR_DATA_LEN 128
#define MAX_VERSION_STRING_LEN 255
#define PLDM_FW_UUID_LEN 16

#define PLDM_COMMON_REQ_LEN 3
#define PLDM_COMMON_RES_LEN 4

#define NCSI_MAX_PAYLOAD 1480

#define MAX_PLDM_MSG_SIZE (NCSI_MAX_PAYLOAD)

#define NUM_PLDM_FW_STR_TYPES 6
#define NUM_COMP_CLASS 14

#define PLDM_CMD_OFFSET 2
#define TFLAG_STATRT_END 0x05
#define PLDM_IID_OFFSET 0
#define PLDM_TYPE_OFFSET 1 // pldm type

#define PLDM_RESP_MASK 0x7f
#define PLDM_CC_OFFSET 3

#define PAYLOAD_LEN 7
#define PAYLOAD_START 20

#define SLEEP_TIME_MS 200
#define MAX_WAIT_CYCLE 1000

#define NCSI_MAX_NL_RESPONSE (sizeof(CTRL_MSG_HDR_t) + NCSI_MAX_PAYLOAD + 4 + 4)

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((unsigned long)&((TYPE*)0)->MEMBER)
#endif

#define MAX_WAIT_CYCLE 1000
#define NUM_PLDM_UPDATE_CDMS 14

static uint8_t gPldm_iid = 0;

// Update Agent (UA) timeout value, in seconds
// as defined in DMTF DSP0267
// https://www.dmtf.org/sites/default/files/standards/documents/DSP0267_1.1.0.pdf
typedef enum
{
    UA_T1 = 5,
    UA_T2 = 90,
    UA_T3 = 1800, // default value for  PLDM_STATE_CHANGE_TIMEOUT_S  --//KUMAR
                  // previous value - 180.
    UA_T4 = 5,
    UA_T5 = 5,
} UA_TO_E;

// IPMI command Completion Codes (IPMI/Section 5.2)
enum
{
    CC_SUCCESS = 0x00,
    CC_INVALID_PARAM = 0x80,
    CC_SEL_ERASE_PROG = 0x81,
    CC_NODE_BUSY = 0xC0,
    CC_INVALID_CMD = 0xC1,
    CC_INVALID_LENGTH = 0xC7,
    CC_PARAM_OUT_OF_RANGE = 0xC9,
    CC_INVALID_DATA_FIELD = 0xCC,
    CC_CAN_NOT_RESPOND = 0xCE,
    CC_NOT_SUPP_IN_CURR_STATE = 0xD5,
    CC_UNSPECIFIED_ERROR = 0xFF,
};

// PLDM types, defined in DMTF DSP0245 v1.2.0
typedef enum pldm_type
{
    PLDM_TYPE_MSG_CTRL_AND_DISCOVERY = 0,
    PLDM_TYPE_SMBIOS = 1,
    PLDM_TYPE_PLATFORM_MONITORING_AND_CTRL = 2,
    PLDM_TYPE_BIOS_CTRL_AND_CFG = 3,
    PLDM_TYPE_FRU_DATA = 4,
    PLDM_TYPE_FIRMWARE_UPDATE = 5,
} PldmType;

// PLDM Firmware update commands
typedef enum pldm_fw_cmds
{
    CMD_REQUEST_UPDATE = 0x10,
    CMD_GET_PACKAGE_DATA = 0x11,
    CMD_GET_DEVICE_METADATA = 0x12,
    CMD_PASS_COMPONENT_TABLE = 0x13,
    CMD_UPDATE_COMPONENT = 0x14,
    CMD_REQUEST_FIRMWARE_DATA = 0x15,
    CMD_TRANSFER_COMPLETE = 0x16,
    CMD_VERIFY_COMPLETE = 0x17,
    CMD_APPLY_COMPLETE = 0x18,
    CMD_GET_META_DATA = 0x19,
    CMD_ACTIVATE_FIRMWARE = 0x1a,
    CMD_GET_STATUS = 0x1b,
    CMD_CANCEL_UPDATE_COMPONENT = 0x1c,
    CMD_CANCEL_UPDATE = 0x1d
} PldmFWCmds;

// Component Classification values   (DSP0267, Table 19)
typedef enum pldm_comp_class
{
    C_CLASS_UNKNOWN = 0,
    C_CLASS_OTHER = 1,
    C_CLASS_DRIVER = 2,
    C_CLASS_CONFIG_SW = 3,
    C_CLASS_APPLICATION_SW = 4,
    C_CLASS_INSTRUMENTATION = 5,
    C_CLASS_FIRMWARE_BIOS = 6,
    C_CLASS_DIAG_SW = 7,
    C_CLASS_OS = 8,
    C_CLASS_MIDDLEWARE = 9,
    C_CLASS_FIRMWARE = 10,
    C_CLASS_BIOS_FCODE = 11,
    C_CLASS_SUPPORT_SERVICE_PACK = 12,
    C_CLASS_SOFTWARE_BUNDLE = 13,
} PldmComponentClass;

extern const char* pldm_comp_class_name[NUM_COMP_CLASS];

#if 0

/* PLDM Firmware completion code*/
const char *pldm_update_cmd_cc_string[NUM_FW_UPDATE_CC] = {
  "NOT_IN_UPDATE_MODE",
  "ALREADY_IN_UPDATE_MODE",
  "DATA_OUT_OF_RANGE",
  "INVALID_TRANSFER_LENTH",
  "INVALID_STATE_FOR_CMD",
  "INCOMPLETE_UPDATE",
  "BUSY_IN_BACKGROUND",
  "CANCEL_PENDING",
  "COMMAND_NOT_EXPECTED",
  "RETRY_REQUEST_FW_DATA",
  "UNABLE_TO_INITIATE_UPDATE",
  "ACTIVATION_NOT_REQUIRED",
  "SELF_CONTAINED_ACTIVATION_NOT_PEMITTED",
  "NO_DEVICE_METADATA",
  "RETRY_REQUEST_UPDATE",
  "NO_PACKAGE_DATA",
  "INVALID_DATA_TRANSFER_HANDLE",
  "INVALID_TRANSFER_OPERATION_FLAG",
};

#endif

/* PLDM component class name*/
const char* pldm_comp_class_name[NUM_COMP_CLASS] = {
    "C_CLASS_UNKNOWN",
    "C_CLASS_OTHER",
    "C_CLASS_DRIVER",
    "C_CLASS_CONFIG_SW",
    "C_CLASS_APPLICATION_SW",
    "C_CLASS_INSTRUMENTATION",
    "C_CLASS_FIRMWARE_BIOS",
    "C_CLASS_DIAG_SW",
    "C_CLASS_OS",
    "C_CLASS_MIDDLEWARE",
    "C_CLASS_FIRMWARE",
    "C_CLASS_BIOS_FCODE",
    "C_CLASS_SUPPORT_SERVICE_PACK",
    "C_CLASS_SOFTWARE_BUNDLE",
};

/* PLDM Firmware Update command name*/
const char* pldm_fw_cmd_string[NUM_PLDM_UPDATE_CDMS] = {
    "CMD_REQUEST_UPDATE",          "CMD_GET_PACKAGE_DATA",
    "CMD_GET_DEVICE_METADATA",     "CMD_PASS_COMPONENT_TABLE",
    "CMD_UPDATE_COMPONENT",        "CMD_REQUEST_FIRMWARE_DATA",
    "CMD_TRANSFER_COMPLETE",       "CMD_VERIFY_COMPLETE",
    "CMD_APPLY_COMPLETE",          "CMD_GET_META_DATA",
    "CMD_ACTIVATE_FIRMWARE",       "CMD_GET_STATUS",
    "CMD_CANCEL_UPDATE_COMPONENT", "CMD_CANCEL_UPDATE",
};

// Version String type values  (DSP0267, Table 20)
typedef enum fw_string_type
{
    str_UNKNOWN = 0,
    str_ASCII = 1,
    str_UTF8 = 2,
    str_UTF16 = 3,
    str_UTF16LE = 4,
    str_UTF16BE = 5,
} fwStrType;

// UUID signature specifying package supports PLDM FW Update
const char PLDM_FW_UUID[PLDM_FW_UUID_LEN] = {0xf0, 0x18, 0x87, 0x8c, 0xcb, 0x7d,
                                             0x49, 0x43, 0x98, 0x00, 0xa0, 0x2f,
                                             0x05, 0x9a, 0xca, 0x02};

// cdb for pldm cmd 0x15 request FW data
typedef struct
{
    uint32_t offset;
    uint32_t length;
} __attribute__((packed)) PLDM_RequestFWData_t;

// cdb for pldm cmd 0x1a activate fw
typedef struct
{
    uint8_t selfContainedActivationRequest;
} __attribute__((packed)) PLDM_ActivateFirmware_t;

// cdb for pldm cmd 0x16 transfer complete
typedef struct
{
    uint8_t transferResult;
} __attribute__((packed)) PLDM_TransferComplete_t;

// cdb for pldm cmd 0x17 verify complete
typedef struct
{
    uint8_t verifyResult;
} __attribute__((packed)) PLDM_VerifyComplete_t;

// cdb for pldm cmd 0x18 apply complete
typedef struct
{
    uint8_t applyResult;
    uint16_t compActivationMethodsModification;
} __attribute__((packed)) PLDM_ApplyComplete_t;

// cdb for pldm cmd 0x10
typedef struct
{
    uint32_t maxTransferSize;
    uint16_t numComponents;
    uint8_t maxOutstandingTransferRequests;
    uint16_t packageDataLength;
    uint8_t componentImageSetVersionStringType;
    uint8_t componentImageSetVersionStringLength;
    char componentImageSetVersionString[MAX_VERSION_STRING_LEN];
} __attribute__((packed)) PLDM_RequestUpdate_t;

typedef struct
{
    uint16_t payload_size;
    unsigned char common[PLDM_COMMON_REQ_LEN];
    unsigned char payload[MAX_PLDM_MSG_SIZE - PLDM_COMMON_REQ_LEN];
} __attribute__((packed)) pldm_cmd_req;

typedef struct
{
    uint16_t resp_size;
    unsigned char common[PLDM_COMMON_RES_LEN];
    unsigned char response[MAX_PLDM_MSG_SIZE - PLDM_COMMON_RES_LEN];
} __attribute__((packed)) pldm_response;

typedef struct
{
    uint8_t transferFlag;
    uint16_t class1;
    uint16_t id;
    uint8_t classIndex;
    uint32_t compStamp;
    uint8_t versionStringType;
    uint8_t versionStringLength;
    char versionString[MAX_VERSION_STRING_LEN];
} __attribute__((packed)) PLDM_PassComponentTable_t;

// cdb for pldm cmd 0x14 Update Component
typedef struct
{
    uint16_t _class;
    uint16_t id;
    uint8_t classIndex;
    uint32_t compStamp;
    uint32_t compSize;
    uint32_t updateOptions;
    uint8_t versionStringType;
    uint8_t versionStringLength;
    char versionString[MAX_VERSION_STRING_LEN];
} __attribute__((packed)) PLDM_UpdateComponent_t;

typedef struct
{
    uint8_t completionCode;
    uint8_t componentCompatibilityResponse;
    uint8_t componentCompatibilityResponseCode;
    uint32_t updateOptionFlagEnabled;
    uint16_t estimatedTimeBeforeSendingRequestFWdata;
} __attribute__((packed)) PLDM_UpdateComponent_Response_t;

// Component image Information, defined in DSP0267 Table 5
// Most of this record is fixed length except for the version string
typedef struct
{
    uint16_t class1;
    uint16_t id;
    uint32_t compStamp;
    uint16_t options;
    uint16_t requestedActivationMethod;
    uint32_t locationOffset;
    uint32_t size;
    uint8_t versionStringType;
    uint8_t versionStringLength;
    char versionString[MAX_VERSION_STRING_LEN];
} __attribute__((packed)) pldm_component_img_info_t;

typedef struct
{
    uint16_t recordLength;
    uint8_t descriptorCnt;
    uint32_t devUpdateOptFlags;
    uint8_t compImgSetVersionStringType;
    uint8_t compImgSetVersionStringLength;
    uint16_t fwDevPkgDataLength;
} __attribute__((packed)) pldm_fw_dev_id_records_fixed_len_t;

typedef struct
{
    uint16_t type;
    uint16_t length;
    char data[PDLM_FW_MAX_DESCRIPTOR_DATA_LEN];
} __attribute__((packed)) record_descriptors_t;

// Firmware device ID record, defined in DSP0267 Table 4
// Each record contains
//      - fixed length portion (11 bytes)
//      - variable length portion:
//          - component bitfield (vraiable length, defined in pkg header info)
//          - version string (up to 255 bytes, length defined in record)
//          - record descriptors (1 or more)
//          - firmware device package data (0 or more bytes)
typedef struct
{
    // pointer to fixed length portion of the records
    pldm_fw_dev_id_records_fixed_len_t* pRecords;

    // pointer to component bitmap, variable length (defined in hdr info)
    uint8_t* pApplicableComponents;

    // pointer to version string, length defined in pRecords
    unsigned char* versionString;

    // array of record descriptors, number of descriptors specified in pRecords
    record_descriptors_t** pRecordDes;

    // pointer to fw device package data (0 or more bytes, defined in pRecords)
    char* pDevPkgData;
} __attribute__((packed)) pldm_fw_dev_id_records_t;

typedef struct
{
    uint8_t uuid[PLDM_FW_UUID_LEN];
    uint8_t headerRevision;
    uint16_t headerSize;
    uint8_t releaseDateTime[13];
    uint16_t componentBitmapBitLength;
    uint8_t versionStringType;
    uint8_t versionStringLength;
    char versionString[MAX_VERSION_STRING_LEN];
} __attribute__((packed)) pldm_fw_pkg_hdr_info_t;

// defines PLDM firmwar package header structure,
//  figure 5 on DSP0267 1.0.0
//  detailed layout in Table 3
typedef struct
{
    unsigned char* rawHdrBuf; // unparsed hdr buffer data bytes

    // Use pointers for acccessing/interpreting hdr buffer above

    // header info area
    pldm_fw_pkg_hdr_info_t* phdrInfo;

    // firmware device id area
    uint8_t devIdRecordCnt;
    pldm_fw_dev_id_records_t**
        pDevIdRecs; // array of pointer to device id records

    // component image information area
    uint16_t componentImageCnt;
    pldm_component_img_info_t**
        pCompImgInfo; // array of pointer to component images info

    // package header checksum area
    uint32_t pkgHdrChksum;
} __attribute__((packed)) pldm_fw_pkg_hdr_t;

#endif
