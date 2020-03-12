#ifndef PDR_H
#define PDR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
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
 *  @param[in/out] repo - pointer to opaque pointer acting as a PDR repo handle
 */
void pldm_pdr_destroy(pldm_pdr *repo);

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
 *  @param[in/out] data - will point to PDR record data (as per DSP0248) on
 *                        return
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
 *  @param[in/out] data - will point to PDR record data (as per DSP0248) on
 *                        return
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
 *  @param[in/out] data - will point to PDR record data (as per DSP0248) on
 *                        return
 *  @param[out] size - *size will be size of PDR record
 *
 *  @return opaque pointer acting as PDR record handle, will be NULL if record
 *  was not found
 */
const pldm_pdr_record *
pldm_pdr_find_record_by_type(const pldm_pdr *repo, uint8_t pdr_type,
			     const pldm_pdr_record *curr_record, uint8_t **data,
			     uint32_t *size);

/* ======================= */
/* FRU Record Set PDR APIs */
/* ======================= */

/** @brief Add a FRU record set PDR record to a PDR repository
 *
 *  @param[in/out] repo - opaque pointer acting as a PDR repo handle
 *  @param[in] terminus_handle - PLDM terminus handle of terminus owning the PDR
 *  record
 *  @param[in] fru_rsi - FRU record set identifier
 *  @param[in] entity_type - entity type of FRU
 *  @param[in] entity_instance_num - entity instance number of FRU
 *  @param[in] container_id - container id of FRU
 *
 *  @return uint32_t - record handle assigned to PDR record
 */
uint32_t pldm_pdr_add_fru_record_set(pldm_pdr *repo, uint16_t terminus_handle,
				     uint16_t fru_rsi, uint16_t entity_type,
				     uint16_t entity_instance_num,
				     uint16_t container_id);

/** @brief Find a FRU record set PDR by FRU record set identifier
 *
 *  @param[in] repo - opaque pointer acting as a PDR repo handle
 *  @param[in] fru_rsi - FRU record set identifier
 *  @param[in] terminus_handle - *terminus_handle will be FRU terminus handle of
 *  found PDR, or 0 if not found
 *  @param[in] entity_type - *entity_type will be FRU entity type of found PDR,
 *  or 0 if not found
 *  @param[in] entity_instance_num - *entity_instance_num will be FRU entity
 *  instance number of found PDR, or 0 if not found
 *  @param[in] container_id - *cintainer_id will be FRU container id of found
 *  PDR, or 0 if not found
 *
 *  @return uint32_t - record handle assigned to PDR record
 */
const pldm_pdr_record *pldm_pdr_fru_record_set_find_by_rsi(
    const pldm_pdr *repo, uint16_t fru_rsi, uint16_t *terminus_handle,
    uint16_t *entity_type, uint16_t *entity_instance_num,
    uint16_t *container_id);

/* =========================== */
/* Entity Association PDR APIs */
/* =========================== */

typedef struct pldm_entity {
	uint16_t entity_type;
	uint16_t entity_instance_num;
	uint16_t entity_container_id;
} __attribute__((packed)) pldm_entity;

enum entity_association_containment_type {
	PLDM_ENTITY_ASSOCIAION_PHYSICAL = 0x0,
	PLDM_ENTITY_ASSOCIAION_LOGICAL = 0x1,
};

/** @struct pldm_entity_association_tree
 *  opaque structure that represents the entity association hierarchy
 */
typedef struct pldm_entity_association_tree pldm_entity_association_tree;

/** @struct pldm_entity_node
 *  opaque structure that represents a node in the entity association hierarchy
 */
typedef struct pldm_entity_node pldm_entity_node;

/** @brief Make a new entity association tree
 *
 *  @return opaque pointer that acts as a handle to the tree; NULL if no
 *  tree could be created
 */
pldm_entity_association_tree *pldm_entity_association_tree_init();

/** @brief Add an entity into the entity association tree
 *
 *  @param[in/out] tree - opaque pointer acting as a handle to the tree
 *  @param[in/out] entity - pointer to the entity to be added. Input has the
 *                          entity type. On output, instance number and the
 *                          container id are populated.
 *  @param[in] parent - pointer to the node that should be the parent of input
 *                      entity. If this is NULL, then the entity is the root
 *  @param[in] association_type - relation with the parent : logical or physical
 *
 *  @return pldm_entity_node* - opaque pointer to added entity
 */
pldm_entity_node *
pldm_entity_association_tree_add(pldm_entity_association_tree *tree,
				 pldm_entity *entity, pldm_entity_node *parent,
				 uint8_t association_type);

/** @brief Visit and note each entity in the entity association tree
 *
 *  @param[in] tree - opaque pointer acting as a handle to the tree
 *  @param[out] entities - pointer to list of pldm_entity's. To be free()'d by
 *                         the caller
 *  @param[out] size - number of pldm_entity's
 */
void pldm_entity_association_tree_visit(pldm_entity_association_tree *tree,
					pldm_entity **entities, size_t *size);

/** @brief Destroy entity association tree
 *
 *  @param[in] tree - opaque pointer acting as a handle to the tree
 */
void pldm_entity_association_tree_destroy(pldm_entity_association_tree *tree);

/** @brief Check if input enity node is a parent
 *
 *  @param[in] node - opaque pointer acting as a handle to an entity node
 *
 *  @return bool true if node is a parent, false otherwise
 */
bool pldm_entity_is_node_parent(pldm_entity_node *node);

/** @brief Convert entity association tree to PDR
 *
 *  @param[in] tree - opaque pointer to entity association tree
 *  @param[in] repo - PDR repo where entity association records should be added
 */
void pldm_entity_association_pdr_add(pldm_entity_association_tree *tree,
				     pldm_pdr *repo);

/** @brief Get number of children of entity
 *
 *  @param[in] node - opaque pointer acting as a handle to an entity node
 *  @param[in] association_type - relation type filter : logical or physical
 *
 *  @return uint8_t number of children
 */
uint8_t pldm_entity_get_num_children(pldm_entity_node *node,
				     uint8_t association_type);

#ifdef __cplusplus
}
#endif

#endif /* PDR_H */
