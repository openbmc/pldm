#include "fw_update_test.hpp"

#include "libncsi/ncsi.h"
#include "libncsi/nl-wrapper.h"
#include "libpldm/base.h"
#include "libpldm/fw_update.h"

#include "libncsi/ncsi_util.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <array>
#include <cstring>
#include <iostream>
#include <vector>

#define MY_DEST_MAC0 0xff
#define MY_DEST_MAC1 0xff
#define MY_DEST_MAC2 0xff
#define MY_DEST_MAC3 0xff
#define MY_DEST_MAC4 0xff
#define MY_DEST_MAC5 0xff

#define DEFAULT_IF "eth0"
#define BUF_SIZ 1500

extern int ncsi_data_len;
extern uint8_t* ncsi_data;

typedef struct
{
    /* Ethernet Header */
    /* NC-SI Control Packet */
    /* Management Controller should set this field to 0x00 */
    unsigned char MC_ID;
    /* For NC-SI 1.0 spec, this field has to set 0x01 */
    unsigned char Header_Revision;
    unsigned char Reserved_1; /* Reserved has to set to 0x00 */
    unsigned char IID;        /* Instance ID */
    unsigned char Command;
    unsigned char Channel_ID;
    /* Payload Length = 12 bits, 4 bits are reserve */
    unsigned short Payload_Length;
    unsigned long Reserved_2;
    unsigned long Reserved_3;
} NCSI_Command_Packet;

typedef struct ether_packet
{
    uint8_t src_addr[6];
    uint8_t dest_addr[6];
    short int ether_type;
    uint8_t payload[112];
} __attribute__((packed)) ETHER_PACKET;

//////

using namespace phosphor::network;
using namespace phosphor::network::ncsi;

// helper function to check if a given UUID is a PLDM FW Package
int isPldmFwUuid(char* uuid)
{
    return (!strncmp(uuid, PLDM_FW_UUID, PLDM_FW_UUID_LEN));
}

/* PLDM firmware string type*/
const char* pldm_str_type_name[NUM_PLDM_FW_STR_TYPES] = {
    "UNKNOWN", "ASCII", "UTF8", "UTF16", "UTF16LE", "UTF16BE",
};

const char* pldm_str_type_to_name(fwStrType strType)
{
    if (strType < 0 || strType >= NUM_PLDM_FW_STR_TYPES ||
        pldm_str_type_name[strType] == NULL)
    {
        return "unknown_str_type";
    }
    return pldm_str_type_name[strType];
}

const char* pldm_comp_class_to_name(PldmComponentClass compClass)
{
    if (compClass < 0 || compClass >= NUM_COMP_CLASS ||
        pldm_comp_class_name[compClass] == NULL)
    {
        return "unknown_classification";
    }
    return pldm_comp_class_name[compClass];
}

void printComponentImgInfo(pldm_fw_pkg_hdr_t* pFwPkgHdr, int index)
{
    int i;
    pldm_component_img_info_t* pComp = pFwPkgHdr->pCompImgInfo[index];

    printf("\n\nComponent[%d] Info\n", index);
    printf("  Component[%d].classification: %d (%s)\n", index, pComp->class1,
           pldm_comp_class_to_name((PldmComponentClass)pComp->class1));
    printf("  Component[%d].identifier: 0x%x\n", index, pComp->id);
    printf("  Component[%d].comparison stamp: 0x%x\n", index, pComp->compStamp);
    printf("  Component[%d].component options: 0x%x\n", index, pComp->options);
    printf("  Component[%d].request update activation method: 0x%x\n", index,
           pComp->requestedActivationMethod);
    printf("  Component[%d].location offset: 0x%x\n", index,
           pComp->locationOffset);
    printf("  Component[%d].component size: 0x%x\n", index, pComp->size);
    printf("  Component[%d].versionStringType: 0x%x (%s)\n", index,
           pComp->versionStringType,
           pldm_str_type_to_name((fwStrType)pComp->versionStringType));
    printf("  Component[%d].versionStringLength: 0x%x\n", index,
           pComp->versionStringLength);

    printf("  Component[%d].versionString: ", index);
    for (i = 0; i < pComp->versionStringLength; ++i)
        printf("%c", pComp->versionString[i]);
    printf("  (");
    for (i = 0; i < pComp->versionStringLength; ++i)
        printf("0x%x ", pComp->versionString[i]);
    printf(")\n");
}

void printDevIdRec(pldm_fw_pkg_hdr_t* pFwPkgHdr, int index)
{
    int i;
    uint8_t compBitFieldLen = pFwPkgHdr->phdrInfo->componentBitmapBitLength / 8;
    pldm_fw_dev_id_records_t* pDevRc = pFwPkgHdr->pDevIdRecs[index];
    uint8_t versionLen = pDevRc->pRecords->compImgSetVersionStringLength;

    printf("\n\nDevice ID[%d] Record Info\n", index);
    printf("  devRec[%d].RecordLength: %d\n", index,
           pDevRc->pRecords->recordLength);
    printf("  devRec[%d].descriptorCnt: 0x%x\n", index,
           pDevRc->pRecords->descriptorCnt);
    printf("  devRec[%d].deviceUpdateOptionFlags: 0x%x\n", index,
           pDevRc->pRecords->devUpdateOptFlags);
    printf("  devRec[%d].compImgSetVersionStringType: 0x%x (%s)\n", index,
           pDevRc->pRecords->compImgSetVersionStringType,
           pldm_str_type_to_name(
               (fwStrType)pDevRc->pRecords->compImgSetVersionStringType));
    printf("  devRec[%d].compImgSetVersionStringLength: 0x%x\n", index,
           pDevRc->pRecords->compImgSetVersionStringLength);
    printf("  devRec[%d].fwDevPkgDataLength: 0x%x\n", index,
           pDevRc->pRecords->fwDevPkgDataLength);

    printf("  devRec[%d].applicableComponents: ", index);
    for (i = 0; i < compBitFieldLen; ++i)
        printf("%d\n", pDevRc->pApplicableComponents[i]);

    printf("  devRec[%d].versionString: ", index);
    for (i = 0; i < versionLen; ++i)
        printf("%c", pDevRc->versionString[i]);
    printf("  (");
    for (i = 0; i < versionLen; ++i)
        printf("0x%x ", pDevRc->versionString[i]);
    printf(")\n");

    // print each record descriptors
    printf("  Record Descriptors\n");
    for (i = 0; i < pDevRc->pRecords->descriptorCnt; ++i)
    {
        printf("    Record Descriptor[%d]\n", i);
        printf("      Type=0x%x", pDevRc->pRecordDes[i]->type);
        printf("      length=0x%x", pDevRc->pRecordDes[i]->length);
        printf("      data=[");
        for (int j = 0; j < pDevRc->pRecordDes[i]->length; ++j)
        {
            printf("%x", pDevRc->pRecordDes[i]->data[j]);
        }
        printf("]\n");
    }
}

void printHdrInfo(pldm_fw_pkg_hdr_info_t* hdr_info, int printVersion)
{
    int i;
    printf("\n\nPLDM Firmware Package Header Info\n");
    printf("  UUID: ");
    for (i = 0; i < 16; ++i)
        printf("%x", hdr_info->uuid[i]);
    printf("    PLDM UUID Y/N? (%C)\n",
           isPldmFwUuid((char*)(hdr_info->uuid)) ? 'Y' : 'N');
    printf("  headerRevision: 0x%x\n", hdr_info->headerRevision);
    printf("  headerSize: 0x%x\n", hdr_info->headerSize);
    printf("  componentBitmapBitLength: 0x%x\n",
           hdr_info->componentBitmapBitLength);
    printf("  versionStringType: 0x%x (%s)\n", hdr_info->versionStringType,
           pldm_str_type_to_name((fwStrType)hdr_info->versionStringType));
    printf("  versionStringLength: 0x%x\n", hdr_info->versionStringLength);

    if (printVersion)
    {
        printf("  version string: ");
        for (i = 0; i < hdr_info->versionStringLength; ++i)
            printf("%c", hdr_info->versionString[i]);
        printf("  (");
        for (i = 0; i < hdr_info->versionStringLength; ++i)
            printf("0x%x ", hdr_info->versionString[i]);
        printf(")\n");
    }
}

int init_device_id_records(pldm_fw_pkg_hdr_t** pFwPkgHdr, int* pOffset)
{
    int i;

    (*pFwPkgHdr)->devIdRecordCnt = (*pFwPkgHdr)->rawHdrBuf[*pOffset];
    *pOffset += sizeof((*pFwPkgHdr)->devIdRecordCnt);
    printf("\n\n Number of Device ID Record in package (devIdRecordCnt) =%d\n",
           (*pFwPkgHdr)->devIdRecordCnt);

    // allocate a look up table of pointers to each devIdRecord
    (*pFwPkgHdr)->pDevIdRecs = (pldm_fw_dev_id_records_t**)calloc(
        (*pFwPkgHdr)->devIdRecordCnt, sizeof(pldm_fw_dev_id_records_t*));
    if (!(*pFwPkgHdr)->pDevIdRecs)
    {
        printf("ERROR: pFwPkgHdr->pDevIdRecs malloc failed, size %d\n",
               sizeof(pldm_fw_dev_id_records_t*) *
                   (*pFwPkgHdr)->devIdRecordCnt);
        return -1;
    }

    for (i = 0; i < (*pFwPkgHdr)->devIdRecordCnt; ++i)
    {
        // pointer to current DeviceRecord we're working on
        (*pFwPkgHdr)->pDevIdRecs[i] = (pldm_fw_dev_id_records_t*)calloc(
            1, sizeof(pldm_fw_dev_id_records_t));
        if (!((*pFwPkgHdr)->pDevIdRecs[i]))
        {
            printf("ERROR: pFwPkgHdr->pDevIdRecs[%d] malloc failed, size %d\n",
                   i, sizeof(pldm_fw_dev_id_records_t));
            return -1;
        }
        pldm_fw_dev_id_records_t* pDevIdRec = (*pFwPkgHdr)->pDevIdRecs[i];

        // "offset" will be updated to track sizeof(current deviceRecord)
        // "suboffset" is used to initialize subfields within current
        // deviceRecord
        //    that are variable length
        int subOffset = *pOffset;

        pDevIdRec->pRecords =
            (pldm_fw_dev_id_records_fixed_len_t*)((*pFwPkgHdr)->rawHdrBuf +
                                                  subOffset);
        subOffset += sizeof(pldm_fw_dev_id_records_fixed_len_t);
        pDevIdRec->pApplicableComponents =
            (uint8_t*)((*pFwPkgHdr)->rawHdrBuf + subOffset);
        // length of pApplicableComponents field is defined in hdr info,
        subOffset += ((*pFwPkgHdr)->phdrInfo->componentBitmapBitLength / 8);
        pDevIdRec->versionString =
            (uint8_t*)((*pFwPkgHdr)->rawHdrBuf + subOffset);
        subOffset += pDevIdRec->pRecords->compImgSetVersionStringLength;

        // allocate a look up table of pointers to each Record Descriptor
        pDevIdRec->pRecordDes = (record_descriptors_t**)calloc(
            pDevIdRec->pRecords->descriptorCnt, sizeof(record_descriptors_t*));
        if (!pDevIdRec->pRecordDes)
        {
            printf("ERROR: pDevIdRec->pRecordDes malloc failed, size %d\n",
                   sizeof(record_descriptors_t*) *
                       pDevIdRec->pRecords->descriptorCnt);
            return -1;
        }
        for (int j = 0; j < pDevIdRec->pRecords->descriptorCnt; ++j)
        {
            pDevIdRec->pRecordDes[j] =
                (record_descriptors_t*)((*pFwPkgHdr)->rawHdrBuf + subOffset);
            subOffset += (offsetof(record_descriptors_t, data) +
                          pDevIdRec->pRecordDes[j]->length);
        }

        printf("DevRec[%d].length=%d, processed=%d\n", i,
               pDevIdRec->pRecords->recordLength, subOffset - *pOffset);
        //		printDevIdRec((*pFwPkgHdr), i);

        // update offset for next deviceRecord
        *pOffset += pDevIdRec->pRecords->recordLength;
    }

    return 0;
}

int init_component_img_info(pldm_fw_pkg_hdr_t** pFwPkgHdr, int* pOffset)
{
    int i;

    (*pFwPkgHdr)->componentImageCnt = (*pFwPkgHdr)->rawHdrBuf[*pOffset];
    *pOffset += sizeof((*pFwPkgHdr)->componentImageCnt);
    printf("\n\n Number of Component in package (componentImageCnt) =%d\n",
           (*pFwPkgHdr)->componentImageCnt);
    // allocate a look up table of pointers to each component image
    (*pFwPkgHdr)->pCompImgInfo = (pldm_component_img_info_t**)calloc(
        (*pFwPkgHdr)->componentImageCnt, sizeof(pldm_component_img_info_t*));
    if (!(*pFwPkgHdr)->pCompImgInfo)
    {
        printf("ERROR: pFwPkgHdr->pCompImgInfo malloc failed, size %d\n",
               sizeof(pldm_component_img_info_t**) *
                   (*pFwPkgHdr)->componentImageCnt);
        return -1;
    }
    for (i = 0; i < (*pFwPkgHdr)->componentImageCnt; ++i)
    {
        (*pFwPkgHdr)->pCompImgInfo[i] =
            (pldm_component_img_info_t*)((*pFwPkgHdr)->rawHdrBuf + *pOffset);
        //		printComponentImgInfo((*pFwPkgHdr), i);

        // move pointer to next Component image, taking int account of variable
        //  version size
        *pOffset += offsetof(pldm_component_img_info_t, versionString) +
                    (*pFwPkgHdr)->pCompImgInfo[i]->versionStringLength;
    }

    return 0;
}

// free up all pointers allocated in pldm_fw_pkg_hdr_t *pFwPkgHdr
void free_pldm_pkg_data(pldm_fw_pkg_hdr_t** pFwPkgHdr)
{
    if ((!pFwPkgHdr) || (!(*pFwPkgHdr)))
        return;

    if ((*pFwPkgHdr)->pDevIdRecs)
    {
        for (int i = 0; i < (*pFwPkgHdr)->devIdRecordCnt; ++i)
        {
            if (((*pFwPkgHdr)->pDevIdRecs[i]) &&
                ((*pFwPkgHdr)->pDevIdRecs[i]->pRecordDes))
            {
                free((*pFwPkgHdr)->pDevIdRecs[i]->pRecordDes);
            }

            if ((*pFwPkgHdr)->pDevIdRecs[i])
            {
                free((*pFwPkgHdr)->pDevIdRecs[i]);
            }
        }
        free((*pFwPkgHdr)->pDevIdRecs);
    }

    if ((*pFwPkgHdr)->pCompImgInfo)
    {
        free((*pFwPkgHdr)->pCompImgInfo);
    }
    if ((*pFwPkgHdr)->rawHdrBuf)
    {
        free((*pFwPkgHdr)->rawHdrBuf);
    }
    free(*pFwPkgHdr);
    return;
}

int init_pkg_hdr_info(char* path, pldm_fw_pkg_hdr_t** pFwPkgHdr, int* pOffset)
{
    FILE* fp;
    int rcnt, size;
    struct stat buf;

    // Open the file exclusively for read
    fp = fopen(path, "r");
    if (!fp)
    {
        printf("ERROR: invalid file path :%s!\n", path);
        return -1;
    }

    stat(path, &buf);
    size = buf.st_size;

    // allocate pointer structure to access fw pkg header
    *pFwPkgHdr = (pldm_fw_pkg_hdr_t*)calloc(1, sizeof(pldm_fw_pkg_hdr_t));
    if (!(*pFwPkgHdr))
    {
        printf("ERROR: pFwPkgHdr malloc failed, size %d\n",
               sizeof(pldm_fw_pkg_hdr_t));
        fclose(fp);
        return -1;
    }

    // allocate actual buffer to store fw package header
    (*pFwPkgHdr)->rawHdrBuf = (unsigned char*)calloc(1, size);
    if (!(*pFwPkgHdr)->rawHdrBuf)
    {
        printf("ERROR: rawHdrBuf malloc failed, size %d\n", size);
        fclose(fp);
        return -1;
    }

    // Move the file pointer to the start and read the entire package header
    fseek(fp, 0, SEEK_SET);
    rcnt = fread((*pFwPkgHdr)->rawHdrBuf, 1, size, fp);
    if (rcnt < size)
    {
        printf("ERROR: rawHdrBuf read, rcnt(%d) < %d", rcnt, size);
        fclose(fp);
        return -1;
    }
    fclose(fp);

    (*pFwPkgHdr)->phdrInfo = (pldm_fw_pkg_hdr_info_t*)(*pFwPkgHdr)->rawHdrBuf;
    //	printHdrInfo((*pFwPkgHdr)->phdrInfo, 1);
    *pOffset += offsetof(pldm_fw_pkg_hdr_info_t, versionString) +
                (*pFwPkgHdr)->phdrInfo->versionStringLength;

    if (!isPldmFwUuid((char*)((*pFwPkgHdr)->phdrInfo->uuid)))
    {
        printf("Not a valid PLDM package, exiting\n");
        return -1;
    }

    return 0;
}

int pldm_parse_fw_pkg(pldm_fw_pkg_hdr_t** pFwPkgHdr, char* path)
{

    int ret = 0;
    int offset = 0;

    // initialize pFwPkgHdr access pointer as fw package header contains
    // multiple variable size fields

    // header info area
    offset = 0;
    ret = init_pkg_hdr_info(path, pFwPkgHdr, &offset);
    if (ret < 0)
    {
        goto error_exit;
    }

    // firmware device id area
    ret = init_device_id_records(pFwPkgHdr, &offset);
    if (ret < 0)
    {
        goto error_exit;
    }

    // component image info area
    ret = init_component_img_info(pFwPkgHdr, &offset);
    if (ret < 0)
    {
        goto error_exit;
    }

    // pkg header checksum
    (*pFwPkgHdr)->pkgHdrChksum = *(uint32_t*)((*pFwPkgHdr)->rawHdrBuf + offset);
    offset += sizeof(uint32_t);
    printf("\n\nPDLM Firmware Package Checksum=0x%x\n",
           (*pFwPkgHdr)->pkgHdrChksum);

    if ((*pFwPkgHdr)->phdrInfo->headerSize != offset)
    {
        printf("ERROR: header size(0x%x) and processed data (0x%x) mismatch\n",
               (*pFwPkgHdr)->phdrInfo->headerSize, offset);
    }

    return 0;

error_exit:
    if (*pFwPkgHdr)
        free_pldm_pkg_data(pFwPkgHdr);
    return -1;
}

void setPldmTimeout(int pldmCmd, int* timeout_sec)
{
    if (pldmCmd == CMD_REQUEST_FIRMWARE_DATA)
    {
        // special timeout value (UA_T2) for Request FW data as specified in
        // DSP0267
        *timeout_sec = UA_T2;
    }
    else
    {
        // default time out is State Change Timeout (in DSP0267, tbl 2 timing
        // spec)
        *timeout_sec = UA_T3;
    }
}

// takes NCSI cmd 0x56 response and returns
// PLDM opcode, as well as the entire cmd field in pldmReq
int ncsiGetPldmCmd(char recvbuf[BUF_SIZ], pldm_cmd_req* pldmReq)
{
    // NCSI response contains at least 4 bytes of Reason code and
    //   Response code.
    if (recvbuf[PAYLOAD_LEN] <= 4)
    {
        // empty response, no PLDM payload
        printf("Recv buf payloadlen %d\n", recvbuf[PAYLOAD_LEN]);
        return -1;
    }

    pldmReq->payload_size =
        recvbuf[PAYLOAD_LEN] - 4; // account for response and reason code
    memcpy((char*)&(pldmReq->common[0]), (char*)&(recvbuf[PAYLOAD_START]),
           pldmReq->payload_size);

    // PLDM cmd byte is the 3rd byte of PLDM header
    return pldmReq->common[PLDM_CMD_OFFSET];
}

static int handlePldmReqFwData(pldm_fw_pkg_hdr_t* pkgHdr, pldm_cmd_req* pCmd,
                               pldm_response* pldmRes)
{
    PLDM_RequestFWData_t* pReqDataCmd = (PLDM_RequestFWData_t*)pCmd->payload;
    // for now assumes  it's always component 0
    unsigned char* pComponent =
        (unsigned char*)(pkgHdr->rawHdrBuf +
                         pkgHdr->pCompImgInfo[0]->locationOffset);
    uint32_t componentSize = pkgHdr->pCompImgInfo[0]->size;

    // calculate how much FW data is left to transfer and if any padding is
    // needed
    int compBytesLeft = componentSize - pReqDataCmd->offset;
    int numPaddingNeeded = pReqDataCmd->length > compBytesLeft
                               ? (pReqDataCmd->length - compBytesLeft)
                               : 0;

    printf("\r%s offset = 0x%x, length = 0x%x, compBytesLeft=%d, numPadding=%d",
           __FUNCTION__, pReqDataCmd->offset, pReqDataCmd->length,
           compBytesLeft, numPaddingNeeded);
    fflush(stdout);

    memcpy(pldmRes->common, pCmd->common, PLDM_COMMON_REQ_LEN);
    // clear Req bit in PLDM response header
    pldmRes->common[PLDM_IID_OFFSET] &= PLDM_RESP_MASK;
    // hard code success for now, need to check length in the future
    pldmRes->common[PLDM_CC_OFFSET] = CC_SUCCESS;

    if (numPaddingNeeded > 0)
    {
        printf("%s %d bytes padding added\n", __FUNCTION__, numPaddingNeeded);
        memcpy(pldmRes->response, pComponent + pReqDataCmd->offset,
               compBytesLeft);
        memset(pldmRes->response + compBytesLeft, 0, numPaddingNeeded);
    }
    else
    {
        memcpy(pldmRes->response, pComponent + pReqDataCmd->offset,
               pReqDataCmd->length);
    }
    pldmRes->resp_size = PLDM_COMMON_RES_LEN + pReqDataCmd->length;

    return 0;
}

static int handlePldmFwTransferComplete(pldm_cmd_req* pCmd, pldm_response* pRes)
{
    PLDM_TransferComplete_t* pReqDataCmd =
        (PLDM_TransferComplete_t*)pCmd->payload;
    int ret = 0;

    if (pReqDataCmd->transferResult != 0)
    {
        printf("Error, transfer failed, err=%d\n", pReqDataCmd->transferResult);
        ret = -1;
    }

    memcpy(pRes->common, pCmd->common, PLDM_COMMON_REQ_LEN);
    // clear Req bit in PLDM response header
    pRes->common[PLDM_IID_OFFSET] &= PLDM_RESP_MASK;
    pRes->common[PLDM_CC_OFFSET] = CC_SUCCESS;
    pRes->resp_size = PLDM_COMMON_RES_LEN;
    return ret;
}

static int handlePldmVerifyComplete(pldm_cmd_req* pCmd, pldm_response* pRes)
{
    PLDM_VerifyComplete_t* pReqDataCmd = (PLDM_VerifyComplete_t*)pCmd->payload;
    int ret = 0;

    if (pReqDataCmd->verifyResult != 0)
    {
        printf("Error, firmware verify failed, err=0x%x\n",
               pReqDataCmd->verifyResult);
        ret = -1;
    }

    memcpy(pRes->common, pCmd->common, PLDM_COMMON_REQ_LEN);
    // clear Req bit in PLDM response header
    pRes->common[PLDM_IID_OFFSET] &= PLDM_RESP_MASK;
    pRes->common[PLDM_CC_OFFSET] = CC_SUCCESS;
    pRes->resp_size = PLDM_COMMON_RES_LEN;
    return ret;
}

static int handlePldmFwApplyComplete(pldm_cmd_req* pCmd, pldm_response* pRes)
{
    PLDM_ApplyComplete_t* pReqDataCmd = (PLDM_ApplyComplete_t*)pCmd->payload;
    int ret = 0;
    if (pReqDataCmd->applyResult != 0)
    {
        printf("Error, firmware apply failed, err=%d\n",
               pReqDataCmd->applyResult);
        ret = -1;
    }

    printf("Apply result = 0x%x, compActivationMethodsModification=0x%x\n",
           pReqDataCmd->applyResult,
           pReqDataCmd->compActivationMethodsModification);

    memcpy(pRes->common, pCmd->common, PLDM_COMMON_REQ_LEN);
    // clear Req bit in PLDM response header
    pRes->common[PLDM_IID_OFFSET] &= PLDM_RESP_MASK;
    pRes->common[PLDM_CC_OFFSET] = CC_SUCCESS;
    pRes->resp_size = PLDM_COMMON_RES_LEN;
    return ret;
}

void genReqCommonFields(PldmType type, PldmFWCmds cmd, uint8_t* common)
{
    uint8_t iid = 0;

    gPldm_iid = (gPldm_iid + 1) & 0x1f;
    ;
    iid = gPldm_iid;

    common[PLDM_IID_OFFSET] = 0x80 | (iid & 0x1f);
    common[PLDM_TYPE_OFFSET] = type;
    common[PLDM_CMD_OFFSET] = cmd;
}

const char* pldm_fw_cmd_to_name(PldmFWCmds cmd)
{
    if (cmd < CMD_REQUEST_UPDATE || cmd > CMD_CANCEL_UPDATE ||
        pldm_fw_cmd_string[cmd - CMD_REQUEST_UPDATE] == NULL)
    {
        return "unknown_pldm_fw_cmd";
    }
    return pldm_fw_cmd_string[cmd - CMD_REQUEST_UPDATE];
}

void dbgPrintCdb(pldm_cmd_req* cdb)
{
    int i = 0;
    printf("%s\n   common:  ", __FUNCTION__);
    for (i = 0; i < 3; ++i)
        printf("%02x", cdb->common[i]);
    printf("\n   payload: ");
    for (i = 0; i < cdb->payload_size; ++i)
    {
        printf("%02x", cdb->payload[i]);
        if ((i % 4 == 3) && (i % 15 != 0))
            printf(" ");
        if (i % 16 == 15)
            printf("\n            ");
    }
    printf("\n");

    /*  printf("\n   iid=%d, cmd=0x%x (%s)\n\n",
                 (cdb->common[PLDM_IID_OFFSET]) & 0x1f,
                 cdb->common[PLDM_CMD_OFFSET],
                 pldm_fw_cmd_to_name(cdb->common[PLDM_CMD_OFFSET]));*/
}

int pldmFwUpdateCmdHandler(pldm_fw_pkg_hdr_t* pkgHdr, pldm_cmd_req* pCmd,
                           pldm_response* pRes)
{
    int cmd = pCmd->common[PLDM_CMD_OFFSET];
    int ret = 0;

    // need to check cmd validity
    switch (cmd)
    {
        case CMD_REQUEST_FIRMWARE_DATA:
            ret = handlePldmReqFwData(pkgHdr, pCmd, pRes);
            break;
        case CMD_TRANSFER_COMPLETE:
            printf("handle CMD_TRANSFER_COMPLETE\n");
            dbgPrintCdb(pCmd);
            ret = handlePldmFwTransferComplete(pCmd, pRes);
            break;
        case CMD_VERIFY_COMPLETE:
            printf("handle CMD_VERIFY_COMPLETE\n");
            dbgPrintCdb(pCmd);
            ret = handlePldmVerifyComplete(pCmd, pRes);
            break;
        case CMD_APPLY_COMPLETE:
            printf("handle CMD_APPLY_COMPLETE\n");
            dbgPrintCdb(pCmd);
            ret = handlePldmFwApplyComplete(pCmd, pRes);
            break;
        default:
            printf("unknown cmd %d\n", cmd);
            dbgPrintCdb(pCmd);
            ret = -1;
            break;
    }
    return ret;
}

int main()
{
    uint8_t componentImageSetVersionString[MAX_VERSION_STRING_LEN];

    pldm_msg* responsePtr;
    uint8_t cc = 0;
    uint16_t val1;
    uint8_t val2;
    uint8_t comp_res;
    uint8_t comp_rescode;
    uint8_t instanceId = 1;
    uint32_t update_options_flag;
    uint16_t EstimatedTimeBefore_sending;
    int pldmCmdStatus = 0;
    int ret = 0;

    int package = 0;
    int channel = 0;
    int ifindex = 2;

    uint8_t opcode = 0x51;
    uint8_t opcode1 = 0x56;
    uint8_t opcode2 = 0x57;

    // firmware package header
    pldm_fw_pkg_hdr_t* pFwPkgHdr;
    pldm_cmd_req pldmReq = {0};
    pldm_response* pldmRes = NULL;
    int pldm_bufsize = 1024;
    char path[] = "/tmp/FB_0000000006.pldm";

    pldmRes = (pldm_response*)calloc(1, sizeof(pldm_response));
    if (!pldmRes)
    {
        printf("%s, Error: failed pldmRes buffer allocation(%d)\n",
               __FUNCTION__, sizeof(pldm_response));
        return -1;
    }

    // parse the headers from the input path
    ret = pldm_parse_fw_pkg(&pFwPkgHdr, path);
    if (ret == -1)
    {
        printf("Error in pldm parse fw pkg.. \n");
        return -1;
    }

    uint16_t packageDataLength =
        pFwPkgHdr->pDevIdRecs[0]->pRecords->fwDevPkgDataLength;

    uint8_t componentImageSetVersionStringType =
        pFwPkgHdr->pDevIdRecs[0]->pRecords->compImgSetVersionStringType;

    size_t componentImageSetVersionStringLength =
        pFwPkgHdr->pDevIdRecs[0]->pRecords->compImgSetVersionStringLength;

    if (componentImageSetVersionStringLength > MAX_VERSION_STRING_LEN)
    {
        componentImageSetVersionStringLength = MAX_VERSION_STRING_LEN;
        printf("%s version length(%d) exceeds max (%d)", __FUNCTION__,
               componentImageSetVersionStringLength, MAX_VERSION_STRING_LEN);
    }

    memcpy(componentImageSetVersionString,
           pFwPkgHdr->pDevIdRecs[0]->versionString,
           componentImageSetVersionStringLength);

    size_t payload_size =
        PLDM_COMMON_REQ_LEN +
        offsetof(PLDM_RequestUpdate_t, componentImageSetVersionString) +
        componentImageSetVersionStringLength;

    uint8_t requestMsg[sizeof(pldm_msg_hdr) + payload_size];
    memset(requestMsg, 0x00, sizeof(pldm_msg_hdr) + payload_size);
    printf("Payload size =%d\n", payload_size);

    auto request = reinterpret_cast<pldm_msg*>(requestMsg);

    auto rc = encode_fw_request_update_req(
        instanceId, pldm_bufsize, pFwPkgHdr->componentImageCnt, 1,
        packageDataLength, componentImageSetVersionStringType,
        componentImageSetVersionStringLength, componentImageSetVersionString,
        componentImageSetVersionStringLength, request);

    printf("Request Msg : ");
    for (int i = 0; i < payload_size; ++i)
    {
        printf("0x%02x ", requestMsg[i]);
    }
    printf("\n");

    if (sendCommand(ifindex, package, channel, opcode, payload_size,
                    requestMsg) < 0)
    {
        printf("Error in sendNcsiCommand Request update\n");
        return -1;
    }

    printf("NCSI Update Comp Response Payload length = %d\n", ncsi_data_len);
    printf("Update Comp Response Payload:\n");
    for (int i = 0; i < ncsi_data_len; ++i)
    {
        printf("0x%02x ", *(ncsi_data + i));
    }
    printf("\n");

    responsePtr = reinterpret_cast<pldm_msg*>((ncsi_data + 20));

    rc = decode_fw_request_update_resp(responsePtr, payload_size, &cc, &val1,
                                       &val2);

    if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
    {
        std::cerr << "Request update command : Response Message Error: "
                  << "rc=" << rc << ",cc=" << (int)cc << std::endl;
        return -1;
    }

    for (int i = 0; i < pFwPkgHdr->componentImageCnt; i++)
    {
        pldm_component_img_info_t* pComp = pFwPkgHdr->pCompImgInfo[0];

        if (pComp == NULL)
        {
            printf("pldm comp image is null \n");
            return -1;
        }

        size_t payload_size =
            PLDM_COMMON_REQ_LEN +
            offsetof(PLDM_PassComponentTable_t, versionString) +
            pComp->versionStringLength;

        uint8_t requestMsg[sizeof(pldm_msg_hdr) + payload_size];
        memset(requestMsg, 0x00, sizeof(pldm_msg_hdr) + payload_size);

        printf("Payload size =%d\n", payload_size);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg);

        rc = encode_fw_pass_component_table_req(
            instanceId, TFLAG_STATRT_END, pComp->class1, pComp->id, 0,
            pComp->compStamp, pComp->versionStringType,
            pComp->versionStringLength, (uint8_t*)pComp->versionString,
            pComp->versionStringLength, request);

        printf("Request Msg : ");
        for (int i = 0; i < payload_size; ++i)
        {
            printf("0x%02x ", requestMsg[i]);
        }
        printf("\n");

        if (sendCommand(ifindex, package, channel, opcode, payload_size,
                        requestMsg) < 0)
        {
            printf("Error in sendNcsiCommand Pass Comp Table.\n");
            return 0;
        }

        printf("NCSI Update Comp Response Payload length = %d\n",
               ncsi_data_len);
        printf("Update Comp Response Payload:\n");
        for (int i = 0; i < ncsi_data_len; ++i)
        {
            printf("0x%02x ", *(ncsi_data + i));
        }
        printf("\n");

        responsePtr = reinterpret_cast<pldm_msg*>((ncsi_data + 20));
        rc = decode_fw_pass_component_table_resp(responsePtr, payload_size, &cc,
                                                 &comp_res, &comp_rescode);

        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Pass Comp Table : Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << std::endl;
            return -1;
        }
    }

    for (int i = 0; i < pFwPkgHdr->componentImageCnt; i++)
    {
        pldm_component_img_info_t* pComp = pFwPkgHdr->pCompImgInfo[0];

        if (pComp == NULL)
        {
            printf("pldm comp image is null \n");
            return -1;
        }

        PLDM_UpdateComponent_t* pCmdPayload =
            (PLDM_UpdateComponent_t*)&(pldmReq);

        genReqCommonFields(PLDM_TYPE_FIRMWARE_UPDATE, CMD_UPDATE_COMPONENT,
                           &(pldmReq.common[0]));

        pCmdPayload->_class = pComp->class1;
        pCmdPayload->id = pComp->id;
        pCmdPayload->classIndex = 0;
        pCmdPayload->compStamp = pComp->compStamp;
        pCmdPayload->compSize = pComp->size;
        pCmdPayload->updateOptions = pComp->options;
        pCmdPayload->versionStringType = pComp->versionStringType;
        pCmdPayload->versionStringLength = pComp->versionStringLength;

        if (pCmdPayload->versionStringLength > MAX_VERSION_STRING_LEN)
        {
            pCmdPayload->versionStringLength = MAX_VERSION_STRING_LEN;
            printf("Update Component : version length(%d) exceeds max (%d)",
                   pCmdPayload->versionStringLength, MAX_VERSION_STRING_LEN);
        }
        memcpy(pCmdPayload->versionString, pComp->versionString,
               pCmdPayload->versionStringLength);

        pldmReq.payload_size = PLDM_COMMON_REQ_LEN +
                               offsetof(PLDM_UpdateComponent_t, versionString) +
                               pCmdPayload->versionStringLength;

        size_t payload_size = PLDM_COMMON_REQ_LEN +
                              offsetof(PLDM_UpdateComponent_t, versionString) +
                              pComp->versionStringLength;

        uint8_t requestMsg[sizeof(pldm_msg_hdr) + payload_size];
        memset(requestMsg, 0x00, sizeof(pldm_msg_hdr) + payload_size);

        printf("Payload size =%d\n", payload_size);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg);

        rc = encode_fw_update_component_req(
            instanceId, pComp->class1, pComp->id, 0, pComp->compStamp,
            pComp->size, pComp->options, pComp->versionStringType,
            pComp->versionStringLength, (uint8_t*)pComp->versionString,
            pComp->versionStringLength, request);

        if (sendCommand(ifindex, package, channel, opcode, payload_size,
                        requestMsg) < 0)
        {
            printf("Error in sendNcsiCommand update component \n");
            return 0;
        }

        printf("NCSI Update Comp Response Payload length = %d\n",
               ncsi_data_len);
        printf("Update Comp Response Payload:\n");
        for (int i = 0; i < ncsi_data_len; ++i)
        {
            printf("0x%02x ", *(ncsi_data + i));
        }
        printf("\n");

        responsePtr = reinterpret_cast<pldm_msg*>((ncsi_data + 20));
        rc = decode_fw_update_component_resp(
            responsePtr, payload_size, &cc, &comp_res, &comp_rescode,
            &update_options_flag, &EstimatedTimeBefore_sending);

        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Update Comp : Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << std::endl;
            return -1;
        }
    }

    // FW data transfer
    int loopCount = 0;
    int idleCnt = 0;
    int pldmCmd = 0;
    int waitTOsec = 0;
    int waitcycle = 0;

    setPldmTimeout(CMD_UPDATE_COMPONENT, &waitTOsec);
    while (idleCnt < (waitTOsec * 1000 / SLEEP_TIME_MS))
    {
        printf("\n04 QueryPendingNcPldmRequestOp, loop=%d\n", loopCount);

        if (sendCommand(ifindex, package, channel, opcode1, 0, NULL) < 0)
        {
            printf("Error in sendNcsiCommand query pending\n");
            return 0;
        }

        printf("NCSI pending command Response Payload length = %d\n",
               ncsi_data_len);
        printf("Pending command Response Payload:\n");
        for (int i = 0; i < ncsi_data_len; ++i)
        {
            printf("0x%02x ", *(ncsi_data + i));
        }
        printf("\n");

        pldmCmd = ncsiGetPldmCmd((char*)ncsi_data, &pldmReq);
        if (pldmCmd == -1)
        {
            printf("No pending command, loop %d\n", idleCnt);
            // sleep(SLEEP_TIME_MS); // wait some time and try again
            // sleep(1);
            idleCnt++;
            //	break; //TODO Remove
            continue;
        }
        else
        {
            idleCnt = 0;
        }
        if ((pldmCmd == CMD_REQUEST_FIRMWARE_DATA) ||
            (pldmCmd == CMD_TRANSFER_COMPLETE) ||
            (pldmCmd == CMD_VERIFY_COMPLETE) || (pldmCmd == CMD_APPLY_COMPLETE))
        {
            setPldmTimeout(pldmCmd, &waitTOsec);
            loopCount++;
            waitcycle = 0;
            pldmCmdStatus =
                pldmFwUpdateCmdHandler(pFwPkgHdr, &pldmReq, pldmRes);

            printf("\n FW Update Cmd Handler :\n");
            printf("Resp size = %d\n", pldmRes->resp_size);

            if (sendCommand(ifindex, package, channel, opcode2,
                            pldmRes->resp_size, pldmRes->common) < 0)
            {
                printf("Error in sendNcsiCommand FW update handler \n");
                return 0;
            }

            printf("NCSI Update cmd Handler Response Payload length = %d\n",
                   ncsi_data_len);
            printf("Update handler Response Payload:\n");
            for (int i = 0; i < ncsi_data_len; ++i)
            {
                printf("0x%02x ", *(ncsi_data + i));
            }
            printf("\n");

            if ((pldmCmd == CMD_APPLY_COMPLETE) || (pldmCmdStatus == -1))
                break;
        }
        else
        {
            printf("unknown PLDM cmd 0x%x\n", pldmCmd);
            break; // TODO Remove
            waitcycle++;
            if (waitcycle >= MAX_WAIT_CYCLE)
            {
                printf("max wait cycle exceeded, exit\n");
                break;
            }
        }
    }

    // only activate FW if update loop exists with good status
    if (!pldmCmdStatus && (pldmCmd == CMD_APPLY_COMPLETE))
    {
        // update successful,  activate FW

        size_t payload_size =
            PLDM_COMMON_REQ_LEN + sizeof(PLDM_ActivateFirmware_t);

        uint8_t requestMsg[sizeof(pldm_msg_hdr) + payload_size];
        memset(requestMsg, 0x00, sizeof(pldm_msg_hdr) + payload_size);

        auto request = reinterpret_cast<pldm_msg*>(requestMsg);

        uint8_t selfCont_Activation_req = 0;
        rc = encode_fw_activate_firmware_req(instanceId,
                                             selfCont_Activation_req, request);

        if (sendCommand(ifindex, package, channel, opcode, payload_size,
                        requestMsg) < 0)
        {
            printf("Error in sendNcsiCommand activate firmware \n");
            return 0;
        }

        printf("NCSI Apply Complete Handler Response Payload length = %d\n",
               ncsi_data_len);
        printf("Apply Complete Response Payload:\n");
        for (int i = 0; i < ncsi_data_len; ++i)
        {
            printf("0x%02x ", *(ncsi_data + i));
        }
        printf("\n");
    }
    else
    {
        printf("PLDM cmd (%d) failed (status %d), abort update\n", pldmCmd,
               pldmCmdStatus);

        // send abort update cmd
        size_t payload_size = PLDM_COMMON_REQ_LEN;

        uint8_t requestMsg[sizeof(pldm_msg_hdr) + payload_size];
        memset(requestMsg, 0x00, sizeof(pldm_msg_hdr) + payload_size);

        auto request = reinterpret_cast<pldm_msg*>(requestMsg);

        rc = encode_fw_cancel_update_req(instanceId, request);

        if (sendCommand(ifindex, package, channel, opcode, payload_size,
                        requestMsg) < 0)
        {
            printf("Error in sendNcsiCommand cancel update\n");
            return 0;
        }

        printf("NCSI cancel update Response Payload length = %d\n",
               ncsi_data_len);
        printf("Cancel Update Response Payload:\n");
        for (int i = 0; i < ncsi_data_len; ++i)
        {
            printf("0x%02x ", *(ncsi_data + i));
        }
        printf("\n");
    }

    if (pldmRes)
        free(pldmRes);

    if (pFwPkgHdr)
        free_pldm_pkg_data(&pFwPkgHdr);
    return 0;
}
