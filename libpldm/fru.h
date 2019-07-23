#ifndef FRU_H
#define FRU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <asm/byteorder.h>
#include <stddef.h>
#include <stdint.h>

#include "base.h"

#define PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES 5

/** @brief PLDM FRU commands
 */
enum pldm_fru_commands {
	PLDM_GET_FRU_RECORD_TABLE_METADATA = 0X01,
	PLDM_GET_FRU_RECORD_TABLE = 0X02,
	PLDM_SET_FRU_RECORD_TABLE = 0X03,
	PLDM_GET_FRU_RECORD_BY_OPTION = 0X04,
};

/** @struct pldm_get_fru_record_table_req
 *
 *  Structure representing PLDM get FRU record table request.
 */
struct pldm_get_fru_record_table_req {
	uint32_t data_transfer_handle;
	uint8_t transfer_operation_flag;
} __attribute__((packed));

/* GetFruRecordTable */

/* @brief Create a PLDM request message for GetFruRecordTable
 *
 * @param[in] instance_id - Message's instance id
 * @param[in] data_transfer_handle - A handle, used to identify a FRU Record
 * Table data transfer
 * @param[in] transfer_operation_flag - A flag that indicates whether this is
 * the start of the transfer
 * @param[in,out] msg - Message will be written to this
 * @return pldm_completion_codes
 * @note  Caller is responsible for memory alloc and dealloc of param
 *        'msg.body.payload'
 */

int encode_get_fru_record_table_req(uint8_t instance_id,
				    uint32_t data_transfer_handle,
				    uint8_t transfer_operation_flag,
				    struct pldm_msg *msg);

#ifdef __cplusplus
}
#endif

#endif
