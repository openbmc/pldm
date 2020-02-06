#ifndef PDR_H
#define PDR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** @struct pldm_pdr
 *  opaque structure that acts as a handle to a PDR repository
 */
typedef struct pldm_pdr pldm_pdr;

/** @struct pldm_pdr_record
 *  opaque structure that acts as a handle to a PDR record
 */
typedef struct pldm_pdr_record pldm_pdr_record;

/* ====================== */
/* Common PDR access APIs */
/* ====================== */

/** @brief Make a new PDR repository
 *
 *  @return opaque pointer that acts as a handle to the repository; NULL if no
 *  repository could be created
 *
 *  @note  Caller may make multiple repositories (for its own PDRs, as well as
 *  for PDRs received by other entities) and can associate the returned handle
 *  to a PLDM terminus id.
 */
pldm_pdr *pldm_pdr_init();

/** @brief Destroy a PDR repository (and free up associated resources)
 *
 *  @param[in/out] repo - pointer to opaque pointer acting as a PDR repo handle;
 *  (*repo) would be set to NULL after the repo is destroyed
 */
void pldm_pdr_destroy(pldm_pdr **repo);

/** @brief Get number of records in a PDR repository
 *
 *  @param[in] repo - opaque pointer acting as a PDR repo handle
 *
 *  @return uint32_t - number of records
 */
uint32_t pldm_pdr_get_record_count(const pldm_pdr *repo);

/** @brief Get size of a PDR repository, in bytes
 *
 *  @param[in] repo - opaque pointer acting as a PDR repo handle
 *
 *  @return uint32_t - size in bytes
 */
uint32_t pldm_pdr_get_repo_size(const pldm_pdr *repo);

/** @brief Add a PDR record to a PDR repository
 *
 *  @param[in/out] repo - opaque pointer acting as a PDR repo handle
 *  @param[in] data - pointer to a PDR record, pointing to a PDR definition as
 *  per DSP0248. This data is memcpy'd.
 *  @param[in] size - size of input PDR record in bytes
 *  @param[in] record_handle - record handle of input PDR record; if this is set
 *  to 0, then a record handle is computed and assigned to this PDR record
 *
 *  @return uint32_t - record handle assigned to PDR record
 */
uint32_t pldm_pdr_add(pldm_pdr *repo, const uint8_t *data, uint32_t size,
		      uint32_t record_handle);

/** @brief Get record handle of a PDR record
 *
 *  @param[in] repo - opaque pointer acting as a PDR repo handle
 *  @param[in] record - opaque pointer acting as a PDR record handle
 *
 *  @return uint32_t - record handle assigned to PDR record; 0 if record is not
 *  found
 */
uint32_t pldm_pdr_get_record_handle(const pldm_pdr *repo,
				    const pldm_pdr_record *record);

/** @brief Find PDR record by record handle
 *
 *  @param[in] repo - opaque pointer acting as a PDR repo handle
 *  @param[in] record_handle - input record handle
 *  @param[in/out] data - *data should be NULL on input, will point to PDR
 *  record data (as per DSP0248) on return
 *  @param[out] size - *size will be size of PDR record
 *  @param[out] next_record_handle - *next_record_handle will be the record
 *  handle of record next to the returned PDR record
 *
 *  @return opaque pointer acting as PDR record handle, will be NULL if record
 *  was not found
 */
const pldm_pdr_record *pldm_pdr_find_record(const pldm_pdr *repo,
					    uint32_t record_handle,
					    uint8_t **data, uint32_t *size,
					    uint32_t *next_record_handle);

/** @brief Get PDR record next to input PDR record
 *
 *  @param[in] repo - opaque pointer acting as a PDR repo handle
 *  @param[in] curr_record - opaque pointer acting as a PDR record handle
 *  @param[in/out] data - *data should be NULL on input, will point to PDR
 *  record data (as per DSP0248) on return
 *  @param[out] size - *size will be size of PDR record
 *  @param[out] next_record_handle - *next_record_handle will be the record
 *  handle of record nect to the returned PDR record
 *
 *  @return opaque pointer acting as PDR record handle, will be NULL if record
 *  was not found
 */
const pldm_pdr_record *
pldm_pdr_get_next_record(const pldm_pdr *repo,
			 const pldm_pdr_record *curr_record, uint8_t **data,
			 uint32_t *size, uint32_t *next_record_handle);

/** @brief Find (first) PDR record by PDR type
 *
 *  @param[in] repo - opaque pointer acting as a PDR repo handle
 *  @param[in] pdr_type - PDR type number as per DSP0248
 *  @param[in] curr_record - opaque pointer acting as a PDR record handle; if
 *  not NULL, then search will begin from this record's next record
 *  @param[in/out] data - *data should be NULL on input, will point to PDR
 *  record data (as per DSP0248) on return
 *  @param[out] size - *size will be size of PDR record
 *
 *  @return opaque pointer acting as PDR record handle, will be NULL if record
 *  was not found
 */
const pldm_pdr_record *
pldm_pdr_find_record_by_type(const pldm_pdr *repo, uint8_t pdr_type,
			     const pldm_pdr_record *curr_record, uint8_t **data,
			     uint32_t *size);

#ifdef __cplusplus
}
#endif

#endif /* PDR_H */
