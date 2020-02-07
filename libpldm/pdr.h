#ifndef PDR_H
#define PDR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** @struct pldm_pdr
 *  structure representing handle to main PDR repository
 */
typedef struct pldm_pdr pldm_pdr;
typedef struct pldm_pdr_record pldm_pdr_record;

/* Common PDR access APIs */
pldm_pdr *pldm_pdr_init();

void pldm_pdr_destroy(pldm_pdr **repo);

uint32_t pldm_pdr_get_record_count(const pldm_pdr *repo);

uint32_t pldm_pdr_get_repo_size(const pldm_pdr *repo);

uint32_t pldm_pdr_add(pldm_pdr *repo, const uint8_t *data, uint32_t size,
		      uint32_t record_handle);

uint32_t pldm_pdr_get_record_handle(const pldm_pdr *repo,
				    const pldm_pdr_record *record);

const pldm_pdr_record *pldm_pdr_get_record(const pldm_pdr *repo,
					   uint32_t record_handle,
					   uint8_t **data, uint32_t *size,
					   uint32_t *next_record_handle);

const pldm_pdr_record *
pldm_pdr_get_next_record(const pldm_pdr *repo,
			 const pldm_pdr_record *curr_record, uint8_t **data,
			 uint32_t *size, uint32_t *next_record_handle);

const pldm_pdr_record *
pldm_pdr_find_record_by_type(const pldm_pdr *repo, uint8_t pdr_type,
			     const pldm_pdr_record *curr_record, uint8_t **data,
			     uint32_t *size);

/* FRU Record Set PDR APIs */
uint32_t pldm_pdr_add_fru_record_set(pldm_pdr *repo, uint16_t terminus_handle,
				     uint16_t fru_rsi, uint16_t entity_type,
				     uint16_t entity_instance_num,
				     uint16_t container_id);

#ifdef __cplusplus
}
#endif

#endif /* PDR_H */
