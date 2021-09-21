#include "pdr.h"
#include "platform.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

typedef struct pldm_pdr {
	uint32_t record_count;
	uint32_t size;
	pldm_pdr_record *first;
	pldm_pdr_record *last;
} pldm_pdr;

static inline uint32_t get_next_record_handle(const pldm_pdr *repo,
					      const pldm_pdr_record *record)
{
	assert(repo != NULL);
	assert(record != NULL);

	if (record == repo->last) {
		return 0;
	}
	return record->next->record_handle;
}

static void add_record(pldm_pdr *repo, pldm_pdr_record *record)
{
	assert(repo != NULL);
	assert(record != NULL);

	if (repo->first == NULL) {
		assert(repo->last == NULL);
		repo->first = record;
		repo->last = record;
	} else {
		repo->last->next = record;
		repo->last = record;
	}
	repo->size += record->size;
	++repo->record_count;
}

static void add_hotplug_record(pldm_pdr *repo, pldm_pdr_record *record,
			       uint32_t prev_record_handle)
{
	/* printf("\nenter add_hotplug_record with
	 record->record_handle=%d",record->record_handle);*/
	// the new record
	// needs to be added after prev_record_handle
	assert(repo != NULL);
	assert(record != NULL);
	record->next = NULL;
	if (repo->first == NULL) {
		assert(repo->last == NULL);
		repo->first = record;
		repo->last = record;
	} else {
		pldm_pdr_record *curr = repo->first;
		while (curr != NULL) {
			if (curr->record_handle == prev_record_handle) {
				break;
			}
			curr = curr->next;
		}
		/* printf("\nadding the fru hotplug here
		 curr->record_handle=%d",curr->record_handle);
		printf("repo-last=%x, curr=%x, curr-next=%x",
		(unsigned int)repo->last, (unsigned int)curr, (unsigned
		int)curr->next);*/
		record->next = curr->next;
		curr->next = record;
		if (record->next == NULL) {
			repo->last = record;
		}
	}
	repo->size += record->size;
	++repo->record_count;
}

static void add_record_after_record_handle(pldm_pdr *repo,
					   pldm_pdr_record *record,
					   uint32_t prev_record_handle)
{
	assert(repo != NULL);
	assert(record != NULL);
	if (repo->first == NULL) {
		assert(repo->last == NULL);
		repo->first = record;
		repo->last = record;
	} else {
		pldm_pdr_record *curr = repo->first;
		while (curr != NULL) {
			if (curr->record_handle == prev_record_handle) {
				break;
			}
			curr = curr->next;
		}
		record->next = curr->next;
		curr->next = record;
		if (record->next == NULL) {
			repo->last = record;
		}
	}
	repo->size += record->size;
	++repo->record_count;
}

static inline uint32_t get_new_record_handle(const pldm_pdr *repo)
{
	assert(repo != NULL);
	uint32_t last_used_hdl =
	    repo->last != NULL ? repo->last->record_handle : 0;
	assert(last_used_hdl != UINT32_MAX);

	return last_used_hdl + 1;
}

static pldm_pdr_record *make_new_record(const pldm_pdr *repo,
					const uint8_t *data, uint32_t size,
					uint32_t record_handle, bool is_remote,
					uint16_t terminus_handle)
{
	assert(repo != NULL);
	assert(size != 0);

	pldm_pdr_record *record = malloc(sizeof(pldm_pdr_record));
	assert(record != NULL);
	record->record_handle =
	    record_handle == 0 ? get_new_record_handle(repo) : record_handle;
	record->size = size;
	record->is_remote = is_remote;
	record->terminus_handle = terminus_handle;
	if (data != NULL) {
		record->data = malloc(size);
		assert(record->data != NULL);
		memcpy(record->data, data, size);
		/* If record handle is 0, that is an indication for this API to
		 * compute a new handle. For that reason, the computed handle
		 * needs to be populated in the PDR header. For a case where the
		 * caller supplied the record handle, it would exist in the
		 * header already.
		 */
		if (!record_handle) {
			struct pldm_pdr_hdr *hdr =
			    (struct pldm_pdr_hdr *)(record->data);
			hdr->record_handle = htole32(record->record_handle);
		}
	}
	record->next = NULL;

	return record;
}

uint32_t pldm_pdr_add(pldm_pdr *repo, const uint8_t *data, uint32_t size,
		      uint32_t record_handle, bool is_remote,
		      uint16_t terminus_handle)
{
	assert(size != 0);
	assert(data != NULL);

	pldm_pdr_record *record = make_new_record(
	    repo, data, size, record_handle, is_remote, terminus_handle);
	add_record(repo, record);

	return record->record_handle;
}

uint32_t pldm_pdr_add_hotplug_record(pldm_pdr *repo, const uint8_t *data,
				     uint32_t size, uint32_t record_handle,
				     bool is_remote,
				     uint32_t prev_record_handle,
				     uint16_t terminus_handle)
{
	assert(size != 0);
	assert(data != NULL);
	// printf("\nenter pldm_pdr_add_hotplug_record with record_handle=%d,
	// prev_record_handle=%d",
	//             record_handle,prev_record_handle);

	pldm_pdr_record *record = make_new_record(
	    repo, data, size, record_handle, is_remote, terminus_handle);
	add_hotplug_record(repo, record, prev_record_handle);

	return record->record_handle;
}

uint32_t pldm_pdr_add_after_prev_record(pldm_pdr *repo, const uint8_t *data,
					uint32_t size, uint32_t record_handle,
					bool is_remote,
					uint32_t prev_record_handle,
					uint16_t terminus_handle)
{
	assert(size != 0);
	assert(data != NULL);

	pldm_pdr_record *record = make_new_record(
	    repo, data, size, record_handle, is_remote, terminus_handle);
	add_record_after_record_handle(repo, record, prev_record_handle);

	return record->record_handle;
}

pldm_pdr *pldm_pdr_init()
{
	pldm_pdr *repo = malloc(sizeof(pldm_pdr));
	assert(repo != NULL);
	repo->record_count = 0;
	repo->size = 0;
	repo->first = NULL;
	repo->last = NULL;

	return repo;
}

void pldm_pdr_destroy(pldm_pdr *repo)
{
	assert(repo != NULL);

	pldm_pdr_record *record = repo->first;
	while (record != NULL) {
		pldm_pdr_record *next = record->next;
		if (record->data) {
			free(record->data);
			record->data = NULL;
		}
		free(record);
		record = next;
	}
	free(repo);
}

const pldm_pdr_record *pldm_pdr_find_record(const pldm_pdr *repo,
					    uint32_t record_handle,
					    uint8_t **data, uint32_t *size,
					    uint32_t *next_record_handle)
{
	assert(repo != NULL);
	assert(data != NULL);
	assert(size != NULL);
	assert(next_record_handle != NULL);

	if (!record_handle && (repo->first != NULL)) {
		record_handle = repo->first->record_handle;
	}
	pldm_pdr_record *record = repo->first;
	while (record != NULL) {
		if (record->record_handle == record_handle) {
			*size = record->size;
			*data = record->data;
			*next_record_handle =
			    get_next_record_handle(repo, record);
			return record;
		}
		record = record->next;
	}

	*size = 0;
	*next_record_handle = 0;
	return NULL;
}

pldm_pdr_record *pldm_pdr_find_last_local_record(const pldm_pdr *repo)
{
	printf("\nenter pldm_pdr_find_last_local_record record_count=%d \n",
	       repo->record_count);
	assert(repo != NULL);
	pldm_pdr_record *curr = repo->first;
	pldm_pdr_record *prev = repo->first;
	uint32_t i = 0;
	// printf("\n first record handle=%d\n",repo->first->record_handle);
	// printf("\nlast record handle=%d,
	// last->next=%d",repo->last->record_handle, (unsigned
	// int)repo->last->next);
	while (curr != NULL) {
		i = curr->record_handle;
		// printf("\ni=%d\n",i);
		if (!(prev->is_remote) && (curr->is_remote)) {
			printf("\nfound record at %d, prev->record_handle=%d\n",
			       i, prev->record_handle);
			return prev;
		}
		prev = curr;
		curr = curr->next;
	}
	if (curr == NULL) {
		printf("\nreached curr as NULL prev->record_handle=%d\n",
		       prev->record_handle);
		return prev;
	}
	return NULL;
}

bool pldm_pdr_find_prev_record_handle(pldm_pdr *repo, uint32_t record_handle,
				      uint32_t *prev_record_handle)
{

	assert(repo != NULL);
	pldm_pdr_record *curr = repo->first;
	pldm_pdr_record *prev = repo->first;
	while (curr != NULL) {
		if (curr->record_handle == record_handle) {
			*prev_record_handle = prev->record_handle;
			return true;
		}
		prev = curr;
		curr = curr->next;
	}
	return false;
}

const pldm_pdr_record *
pldm_pdr_get_next_record(const pldm_pdr *repo,
			 const pldm_pdr_record *curr_record, uint8_t **data,
			 uint32_t *size, uint32_t *next_record_handle)
{
	assert(repo != NULL);
	assert(curr_record != NULL);
	assert(data != NULL);
	assert(size != NULL);
	assert(next_record_handle != NULL);

	if (curr_record == repo->last) {
		*data = NULL;
		*size = 0;
		*next_record_handle = get_next_record_handle(repo, curr_record);
		return NULL;
	}

	*next_record_handle = get_next_record_handle(repo, curr_record->next);
	*data = curr_record->next->data;
	*size = curr_record->next->size;
	return curr_record->next;
}

const pldm_pdr_record *
pldm_pdr_find_record_by_type(const pldm_pdr *repo, uint8_t pdr_type,
			     const pldm_pdr_record *curr_record, uint8_t **data,
			     uint32_t *size)
{
	assert(repo != NULL);

	pldm_pdr_record *record = repo->first;
	if (curr_record != NULL) {
		record = curr_record->next;
	}
	while (record != NULL) {
		struct pldm_pdr_hdr *hdr = (struct pldm_pdr_hdr *)record->data;
		if (hdr->type == pdr_type) {
			if (data && size) {
				*size = record->size;
				*data = record->data;
			}
			return record;
		}
		record = record->next;
	}

	if (size) {
		*size = 0;
	}
	return NULL;
}

uint32_t pldm_pdr_get_record_count(const pldm_pdr *repo)
{
	assert(repo != NULL);

	return repo->record_count;
}

uint32_t pldm_pdr_get_repo_size(const pldm_pdr *repo)
{
	assert(repo != NULL);

	return repo->size;
}

uint32_t pldm_pdr_get_record_handle(const pldm_pdr *repo,
				    const pldm_pdr_record *record)
{
	assert(repo != NULL);
	assert(record != NULL);

	return record->record_handle;
}

inline bool pldm_pdr_record_is_remote(const pldm_pdr_record *record)
{
	assert(record != NULL);

	return record->is_remote;
}

uint32_t pldm_pdr_add_fru_record_set(pldm_pdr *repo, uint16_t terminus_handle,
				     uint16_t fru_rsi, uint16_t entity_type,
				     uint16_t entity_instance_num,
				     uint16_t container_id,
				     uint32_t bmc_record_handle)
{
	uint32_t size = sizeof(struct pldm_pdr_hdr) +
			sizeof(struct pldm_pdr_fru_record_set);
	uint8_t data[size];
	bool hotplug = false;
	pldm_pdr_record *prev = repo->first;

	if (bmc_record_handle == 0xFFFF) // handle hot plug
	{
		printf("\nadding a hot plugged record, record count=%d before "
		       "adding\n",
		       repo->record_count);
		hotplug = true;
		pldm_pdr_record *curr = repo->first;
		while (curr != NULL) {
			if (!prev->is_remote && curr->is_remote) {
				break;
			}
			prev = curr;
			curr = curr->next;
		}
		bmc_record_handle = prev->record_handle + 1;
		printf("\ngenerated record handle=%d", bmc_record_handle);
	}

	struct pldm_pdr_hdr *hdr = (struct pldm_pdr_hdr *)&data;
	hdr->version = 1;
	hdr->record_handle = bmc_record_handle;
	hdr->type = PLDM_PDR_FRU_RECORD_SET;
	hdr->record_change_num = 0;
	hdr->length = htole16(sizeof(struct pldm_pdr_fru_record_set));
	struct pldm_pdr_fru_record_set *fru =
	    (struct pldm_pdr_fru_record_set *)((uint8_t *)hdr +
					       sizeof(struct pldm_pdr_hdr));
	fru->terminus_handle = htole16(terminus_handle);
	fru->fru_rsi = htole16(fru_rsi);
	fru->entity_type = htole16(entity_type);
	fru->entity_instance = htole16(entity_instance_num);
	fru->container_id = htole16(container_id);

	if (hotplug) {
		return pldm_pdr_add_hotplug_record(
		    repo, data, size, bmc_record_handle, false,
		    prev->record_handle, fru->terminus_handle);
	} else {
		return pldm_pdr_add(repo, data, size, bmc_record_handle, false,
				    fru->terminus_handle);
	}
}

const pldm_pdr_record *pldm_pdr_fru_record_set_find_by_rsi(
    const pldm_pdr *repo, uint16_t fru_rsi, uint16_t *terminus_handle,
    uint16_t *entity_type, uint16_t *entity_instance_num,
    uint16_t *container_id, bool is_remote)
{
	assert(terminus_handle != NULL);
	assert(entity_type != NULL);
	assert(entity_instance_num != NULL);
	assert(container_id != NULL);

	uint8_t *data = NULL;
	uint32_t size = 0;
	const pldm_pdr_record *curr_record = pldm_pdr_find_record_by_type(
	    repo, PLDM_PDR_FRU_RECORD_SET, NULL, &data, &size);
	while (curr_record != NULL) {
		struct pldm_pdr_fru_record_set *fru =
		    (struct pldm_pdr_fru_record_set
			 *)(data + sizeof(struct pldm_pdr_hdr));
		if (fru->fru_rsi == htole16(fru_rsi) &&
		    curr_record->is_remote == is_remote) {
			*terminus_handle = le16toh(fru->terminus_handle);
			*entity_type = le16toh(fru->entity_type);
			*entity_instance_num = le16toh(fru->entity_instance);
			*container_id = le16toh(fru->container_id);
			return curr_record;
		}
		data = NULL;
		curr_record = pldm_pdr_find_record_by_type(
		    repo, PLDM_PDR_FRU_RECORD_SET, curr_record, &data, &size);
	}

	*terminus_handle = 0;
	*entity_type = 0;
	*entity_instance_num = 0;
	*container_id = 0;

	return NULL;
}

uint32_t pldm_pdr_remove_fru_record_set_by_rsi(pldm_pdr *repo, uint16_t fru_rsi,
					       bool is_remote)
{
	assert(repo != NULL);

	uint32_t delete_hdl = 0;
	pldm_pdr_record *record = repo->first;
	pldm_pdr_record *prev = NULL;
	while (record != NULL) {
		pldm_pdr_record *next = record->next;
		struct pldm_pdr_hdr *hdr = (struct pldm_pdr_hdr *)record->data;
		if ((record->is_remote == is_remote) &&
		    hdr->type == PLDM_PDR_FRU_RECORD_SET) {
			struct pldm_pdr_fru_record_set *fru =
			    (struct pldm_pdr_fru_record_set
				 *)((uint8_t *)record->data +
				    sizeof(struct pldm_pdr_hdr));
			if (fru->fru_rsi == fru_rsi) {
				delete_hdl = hdr->record_handle;
				if (repo->first == record) {
					repo->first = next;
				} else {
					prev->next = next;
				}
				if (repo->last == record) {
					repo->last = prev;
					prev->next = NULL; // sm00
				}
				--repo->record_count;
				repo->size -= record->size;
				if (record->data) {
					free(record->data);
				}
				free(record);
				break;
			} else {
				prev = record;
			}
		} else {
			prev = record;
		}
		record = next;
	}
	return delete_hdl;
}

void pldm_pdr_update_TL_pdr(const pldm_pdr *repo, uint16_t terminusHandle,
			    uint8_t tid, uint8_t tlEid, bool validBit)
{
	uint8_t *outData = NULL;
	uint32_t size = 0;
	const pldm_pdr_record *record;
	record = pldm_pdr_find_record_by_type(repo, PLDM_TERMINUS_LOCATOR_PDR,
					      NULL, &outData, &size);

	do {
		if (record != NULL) {
			struct pldm_terminus_locator_pdr *pdr =
			    (struct pldm_terminus_locator_pdr *)outData;
			struct pldm_terminus_locator_type_mctp_eid *value =
			    (struct pldm_terminus_locator_type_mctp_eid *)
				pdr->terminus_locator_value;
			if (pdr->terminus_handle == terminusHandle &&
			    pdr->tid == tid && value->eid == tlEid) {
				pdr->validity = validBit;
				break;
			}
		}
		record = pldm_pdr_find_record_by_type(
		    repo, PLDM_TERMINUS_LOCATOR_PDR, record, &outData, &size);
	} while (record);
}

void pldm_delete_by_record_handle(pldm_pdr *repo, uint32_t record_handle,
				  bool is_remote)
{
	assert(repo != NULL);

	pldm_pdr_record *record = repo->first;
	pldm_pdr_record *prev = NULL;
	while (record != NULL) {
		pldm_pdr_record *next = record->next;
		struct pldm_pdr_hdr *hdr = (struct pldm_pdr_hdr *)record->data;
		if ((record->is_remote == is_remote) &&
		    (hdr->record_handle == record_handle)) {
			if (repo->first == record) {
				repo->first = next;
			} else {
				prev->next = next;
			}
			if (repo->last == record) {
				repo->last = prev;
			}
			if (record->data) {
				free(record->data);
			}
			--repo->record_count;
			repo->size -= record->size;
			free(record);
			break;
		} else {
			prev = record;
		}
		record = next;
	}
}

typedef struct pldm_entity_association_tree {
	pldm_entity_node *root;
	uint16_t last_used_container_id;
} pldm_entity_association_tree;

typedef struct pldm_entity_node {
	pldm_entity entity;
	pldm_entity parent;
	uint16_t host_container_id;
	pldm_entity_node *first_child;
	pldm_entity_node *next_sibling;
	uint8_t association_type;
} pldm_entity_node;

static inline uint16_t next_container_id(pldm_entity_association_tree *tree)
{
	assert(tree != NULL);
	assert(tree->last_used_container_id != UINT16_MAX);

	return ++tree->last_used_container_id;
}

pldm_entity pldm_entity_extract(pldm_entity_node *node)
{
	assert(node != NULL);

	return node->entity;
}

uint16_t pldm_extract_host_container_id(const pldm_entity_node *entity)
{
	assert(entity != NULL);

	return entity->host_container_id;
}

pldm_entity_association_tree *pldm_entity_association_tree_init()
{
	pldm_entity_association_tree *tree =
	    malloc(sizeof(pldm_entity_association_tree));
	assert(tree != NULL);
	tree->root = NULL;
	tree->last_used_container_id = 0;

	return tree;
}

static pldm_entity_node *
find_insertion_at(pldm_entity_node *start,
		  uint16_t entity_type) //,uint16_t *instance) //sm00
{
	/*printf("\nenter find_insertion_at start->entity.entity_type=%d and
	   entity_type=%d", start->entity.entity_type,entity_type); */
	assert(start != NULL);

	/* Insert after the the last node that matches the input entity type, or
	 * at the end if no such match occurrs
	 */
	while (start->next_sibling != NULL) {
		uint16_t this_type = start->entity.entity_type;
		pldm_entity_node *next = start->next_sibling;
		if (this_type == entity_type &&
		    (this_type != next->entity.entity_type)) {
			break;
		}
		start = start->next_sibling;
	}
	/*while (start->next_sibling != NULL) //sm00
	{
	    uint16_t this_type = start->entity.entity_type;
	    if (this_type == entity_type)
	    {
		*instance = start->entity.entity_instance_num;
	    }
	    start = start->next_sibling;
	}*/
	// printf("\nfrom find_insertion_at returning instance=%d",*instance);
	return start;
}

pldm_entity_node *pldm_entity_association_tree_add(
    pldm_entity_association_tree *tree, pldm_entity *entity,
    uint16_t entity_instance_number, pldm_entity_node *parent,
    uint8_t association_type, bool is_remote, bool is_update_contanier_id)
{
	/*printf("\nenter pldm_entity_association_tree_add"); */
	if (parent) {
		/*     printf("\nenter pldm_entity_association_tree_add with
		 * parent entity_type=%d,
		 * entity_instance_num=%d,ntity_container_id=%d",
		 * parent->entity.entity_type,
		 * parent->entity.entity_instance_num,
		 * parent->entity.entity_container_id);*/
	}
	assert(tree != NULL);
	assert(entity != NULL);
	// uint16_t instance = 0; //sm00

	if (entity_instance_number != 0xFFFF && parent != NULL) {
		pldm_entity node;
		node.entity_type = entity->entity_type;
		node.entity_instance_num = entity_instance_number;
		if (pldm_is_current_parent_child(parent, &node)) {
			return NULL;
		}
	}

	assert(association_type == PLDM_ENTITY_ASSOCIAION_PHYSICAL ||
	       association_type == PLDM_ENTITY_ASSOCIAION_LOGICAL);
	pldm_entity_node *node = malloc(sizeof(pldm_entity_node));
	assert(node != NULL);
	node->first_child = NULL;
	node->next_sibling = NULL;
	node->parent.entity_type = 0;
	node->parent.entity_instance_num = 0;
	node->parent.entity_container_id = 0;
	node->entity.entity_type = entity->entity_type;
	node->entity.entity_instance_num =
	    entity_instance_number != 0xFFFF ? entity_instance_number : 1;
	node->association_type = association_type;
	node->host_container_id = 0;

	if (tree->root == NULL) {
		assert(parent == NULL);
		tree->root = node;
		/* container_id 0 here indicates this is the top-most entry */
		node->entity.entity_container_id = 0;
		node->host_container_id = node->entity.entity_container_id;
	} else if (parent != NULL && parent->first_child == NULL) {
		parent->first_child = node;
		node->parent = parent->entity;
		if (is_remote) {
			node->host_container_id = entity->entity_container_id;
			node->entity.entity_container_id =
			    is_update_contanier_id
				? next_container_id(tree)
				: entity->entity_container_id;

		} else {
			node->entity.entity_container_id =
			    is_update_contanier_id
				? next_container_id(tree)
				: entity->entity_container_id;
			node->host_container_id =
			    node->entity.entity_container_id;
		}
	} else {
		/* printf("\ncreating the node now\n"); */
		pldm_entity_node *start =
		    parent == NULL ? tree->root : parent->first_child;
		pldm_entity_node *prev =
		    find_insertion_at(start, entity->entity_type); // sm00
		// find_insertion_at(start, entity->entity_type,&instance);
		// printf("\nreturned from find_insertion_at with
		// instance=%d",instance);
		assert(prev != NULL);
		pldm_entity_node *next = prev->next_sibling;
		if (prev->entity.entity_type == entity->entity_type) {
			assert(prev->entity.entity_instance_num != UINT16_MAX);
			node->entity.entity_instance_num =
			    entity_instance_number != 0xFFFF
				? entity_instance_number
				: prev->entity.entity_instance_num + 1;
		}
		/*else //sm00
		{
		    printf("\ncm node add \n");
		    node->entity.entity_instance_num = instance + 1;
		}*/
		prev->next_sibling = node;
		node->parent = prev->parent;
		node->next_sibling = next;
		node->entity.entity_container_id =
		    prev->entity.entity_container_id;
		node->host_container_id = entity->entity_container_id;
	}
	entity->entity_instance_num = node->entity.entity_instance_num;
	if (is_update_contanier_id) {
		entity->entity_container_id = node->entity.entity_container_id;
	}

	/*printf("\nexit pldm_entity_association_tree_add"); */
	return node;
}

static void get_num_nodes(pldm_entity_node *node, size_t *num)
{
	if (node == NULL) {
		return;
	}

	++(*num);
	get_num_nodes(node->next_sibling, num);
	get_num_nodes(node->first_child, num);
}

static void entity_association_tree_visit(pldm_entity_node *node,
					  pldm_entity *entities, size_t *index)
{
	if (node == NULL) {
		return;
	}

	pldm_entity *entity = &entities[*index];
	++(*index);
	entity->entity_type = node->entity.entity_type;
	entity->entity_instance_num = node->entity.entity_instance_num;
	entity->entity_container_id = node->entity.entity_container_id;
	printf("\n\n\nentity_type=%d, instance_num=%d,container_id=%d",
	       entity->entity_type, entity->entity_instance_num,
	       entity->entity_container_id);
	if (node->next_sibling) {
		printf("\nsibling type=%d, instance=%d, container=%d",
		       node->next_sibling->entity.entity_type,
		       node->next_sibling->entity.entity_instance_num,
		       node->next_sibling->entity.entity_container_id);
	}
	if (node->first_child) {
		printf("\nfirst child type=%d, instance=%d, container=%d",
		       node->first_child->entity.entity_type,
		       node->first_child->entity.entity_instance_num,
		       node->first_child->entity.entity_container_id);
	}
	entity_association_tree_visit(node->next_sibling, entities, index);
	entity_association_tree_visit(node->first_child, entities, index);
}

void pldm_entity_association_tree_visit(pldm_entity_association_tree *tree,
					pldm_entity **entities, size_t *size)
{
	/*  printf("\nenter pldm_entity_association_tree_visit with
	 * tree=%p\n",(void*)tree); */
	assert(tree != NULL);

	*size = 0;
	if (tree->root == NULL) {
		return;
	}

	get_num_nodes(tree->root, size);
	*entities = malloc(*size * sizeof(pldm_entity));
	size_t index = 0;
	entity_association_tree_visit(tree->root, *entities, &index);
}

static void entity_association_tree_destroy(pldm_entity_node *node)
{
	if (node == NULL) {
		return;
	}

	entity_association_tree_destroy(node->next_sibling);
	entity_association_tree_destroy(node->first_child);
	free(node);
}

void pldm_entity_association_tree_destroy(pldm_entity_association_tree *tree)
{
	assert(tree != NULL);

	entity_association_tree_destroy(tree->root);
	free(tree);
}

void pldm_entity_association_tree_delete_node(
    pldm_entity_association_tree *tree, pldm_entity entity)
{
	//	printf("\nenter pldm_entity_association_tree_delete_node");
	pldm_entity_node *node = NULL;
	pldm_find_entity_ref_in_tree(tree, entity, &node);
	// sm00
	/* printf(
		    "\nfound node to delete "
		    "node->entity.entity_type=%d,node->entity.entity_instance_num=%d",
		    node->entity.entity_type,
	   node->entity.entity_instance_num);*/
	pldm_entity_node *parent = NULL;
	pldm_find_entity_ref_in_tree(tree, node->parent, &parent);
	//	printf("\nfound parent");
	pldm_entity_node *start = parent->first_child;
	pldm_entity_node *prev = parent->first_child;
	while (start != NULL) {
		pldm_entity current_entity;
		current_entity.entity_type = start->entity.entity_type;
		current_entity.entity_instance_num =
		    start->entity.entity_instance_num;
		current_entity.entity_container_id =
		    start->entity.entity_container_id;
		if (current_entity.entity_type == entity.entity_type &&
		    current_entity.entity_instance_num ==
			entity.entity_instance_num &&
		    current_entity.entity_container_id ==
			entity.entity_container_id) {
			break;
		}
		prev = start;
		start = start->next_sibling;
	}
	if (start == parent->first_child) {
		parent->first_child = start->next_sibling;
	} else {
		prev->next_sibling = start->next_sibling;
	}
	start->next_sibling = NULL;
	entity_association_tree_destroy(node);
}

inline bool pldm_entity_is_node_parent(pldm_entity_node *node)
{
	assert(node != NULL);

	return node->first_child != NULL;
}

inline pldm_entity pldm_entity_get_parent(pldm_entity_node *node)
{
	assert(node != NULL);

	return node->parent;
}

inline bool pldm_entity_is_exist_parent(pldm_entity_node *node)
{
	assert(node != NULL);

	if (node->parent.entity_type == 0 &&
	    node->parent.entity_instance_num == 0 &&
	    node->parent.entity_container_id == 0) {
		return false;
	}

	return true;
}

uint8_t pldm_entity_get_num_children(pldm_entity_node *node,
				     uint8_t association_type)
{
	assert(node != NULL);
	assert(association_type == PLDM_ENTITY_ASSOCIAION_PHYSICAL ||
	       association_type == PLDM_ENTITY_ASSOCIAION_LOGICAL);

	size_t count = 0;
	pldm_entity_node *curr = node->first_child;
	while (curr != NULL) {
		if (curr->association_type == association_type) {
			++count;
		}
		curr = curr->next_sibling;
	}

	assert(count < UINT8_MAX);
	return count;
}

bool pldm_is_current_parent_child(pldm_entity_node *parent, pldm_entity *node)
{
	assert(parent != NULL);
	assert(node != NULL);

	pldm_entity_node *curr = parent->first_child;
	while (curr != NULL) {
		if (node->entity_type == curr->entity.entity_type &&
		    node->entity_instance_num ==
			curr->entity.entity_instance_num) {

			return true;
		}
		curr = curr->next_sibling;
	}

	return false;
}

static void _entity_association_pdr_add_entry(pldm_entity_node *curr,
					      pldm_pdr *repo, uint16_t size,
					      uint8_t contained_count,
					      uint8_t association_type,
					      bool is_remote,
					      uint16_t terminus_handle)
{
	uint8_t pdr[size];
	uint8_t *start = pdr;

	struct pldm_pdr_hdr *hdr = (struct pldm_pdr_hdr *)start;
	hdr->version = 1;
	hdr->record_handle = 0;
	hdr->type = PLDM_PDR_ENTITY_ASSOCIATION;
	hdr->record_change_num = 0;
	hdr->length = htole16(size - sizeof(struct pldm_pdr_hdr));
	start += sizeof(struct pldm_pdr_hdr);

	uint16_t *container_id = (uint16_t *)start;
	*container_id = htole16(curr->first_child->entity.entity_container_id);
	start += sizeof(uint16_t);
	*start = association_type;
	start += sizeof(uint8_t);

	pldm_entity *entity = (pldm_entity *)start;
	entity->entity_type = htole16(curr->entity.entity_type);
	entity->entity_instance_num = htole16(curr->entity.entity_instance_num);
	entity->entity_container_id = htole16(curr->entity.entity_container_id);
	start += sizeof(pldm_entity);

	*start = contained_count;
	start += sizeof(uint8_t);

	pldm_entity_node *node = curr->first_child;
	while (node != NULL) {
		if (node->association_type == association_type) {
			pldm_entity *entity = (pldm_entity *)start;
			entity->entity_type = htole16(node->entity.entity_type);
			entity->entity_instance_num =
			    htole16(node->entity.entity_instance_num);
			entity->entity_container_id =
			    htole16(node->entity.entity_container_id);
			start += sizeof(pldm_entity);
		}
		node = node->next_sibling;
	}

	pldm_pdr_add(repo, pdr, size, 0, is_remote, terminus_handle);
}

static void entity_association_pdr_add_entry(pldm_entity_node *curr,
					     pldm_pdr *repo, bool is_remote,
					     uint16_t terminus_handle)
{
	uint8_t num_logical_children =
	    pldm_entity_get_num_children(curr, PLDM_ENTITY_ASSOCIAION_LOGICAL);
	uint8_t num_physical_children =
	    pldm_entity_get_num_children(curr, PLDM_ENTITY_ASSOCIAION_PHYSICAL);

	if (num_logical_children) {
		uint16_t logical_pdr_size =
		    sizeof(struct pldm_pdr_hdr) + sizeof(uint16_t) +
		    sizeof(uint8_t) + sizeof(pldm_entity) + sizeof(uint8_t) +
		    (num_logical_children * sizeof(pldm_entity));
		_entity_association_pdr_add_entry(
		    curr, repo, logical_pdr_size, num_logical_children,
		    PLDM_ENTITY_ASSOCIAION_LOGICAL, is_remote, terminus_handle);
	}

	if (num_physical_children) {
		uint16_t physical_pdr_size =
		    sizeof(struct pldm_pdr_hdr) + sizeof(uint16_t) +
		    sizeof(uint8_t) + sizeof(pldm_entity) + sizeof(uint8_t) +
		    (num_physical_children * sizeof(pldm_entity));
		_entity_association_pdr_add_entry(
		    curr, repo, physical_pdr_size, num_physical_children,
		    PLDM_ENTITY_ASSOCIAION_PHYSICAL, is_remote,
		    terminus_handle);
	}
}

bool is_present(pldm_entity entity, pldm_entity **entities, size_t num_entities)
{
	if (entities == NULL || num_entities == 0) {
		return true;
	}
	size_t i = 0;
	while (i < num_entities) {
		if ((*entities + i)->entity_type == entity.entity_type) {
			return true;
		}
		i++;
	}
	return false;
}

static void entity_association_pdr_add(pldm_entity_node *curr, pldm_pdr *repo,
				       pldm_entity **entities,
				       size_t num_entities, bool is_remote,
				       uint16_t terminus_handle)
{
	if (curr == NULL) {
		return;
	}
	bool to_add = true;
	to_add = is_present(curr->entity, entities, num_entities);
	if (to_add) {
		entity_association_pdr_add_entry(curr, repo, is_remote,
						 terminus_handle);
	}
	entity_association_pdr_add(curr->next_sibling, repo, entities,
				   num_entities, is_remote, terminus_handle);
	entity_association_pdr_add(curr->first_child, repo, entities,
				   num_entities, is_remote, terminus_handle);
}

void pldm_entity_association_pdr_add(pldm_entity_association_tree *tree,
				     pldm_pdr *repo, bool is_remote,
				     uint16_t terminus_handle)
{
	assert(tree != NULL);
	assert(repo != NULL);

	entity_association_pdr_add(tree->root, repo, NULL, 0, is_remote,
				   terminus_handle);
}

void pldm_entity_association_pdr_add_from_node(
    pldm_entity_node *node, pldm_pdr *repo, pldm_entity **entities,
    size_t num_entities, bool is_remote, uint16_t terminus_handle)
{
	assert(repo != NULL);

	entity_association_pdr_add(node, repo, entities, num_entities,
				   is_remote, terminus_handle);
}

uint32_t find_record_handle_by_contained_entity(pldm_pdr *repo,
						pldm_entity entity,
						bool is_remote)
{
	/*printf("\nenter find_record_handle_by_contained_entity with "
		"entity_type=%d, entity_instance_num=%d, "
		"entity_container_id=%d,is_remote=%d",
		entity.entity_type, entity.entity_instance_num,
		entity.entity_container_id, is_remote);*/
	uint32_t record_handle = 0;
	bool found = false;
	assert(repo != NULL);
	pldm_pdr_record *record = repo->first;
	while (record != NULL && !found) {
		pldm_pdr_record *next = record->next;
		struct pldm_pdr_hdr *hdr = (struct pldm_pdr_hdr *)record->data;
		if ((record->is_remote == is_remote) &&
		    hdr->type == PLDM_PDR_ENTITY_ASSOCIATION) {
			/*	printf("\ngot one PLDM_PDR_ENTITY_ASSOCIATION "
				       "record_handle=%d",
				       record->record_handle);*/
			struct pldm_pdr_entity_association *pdr =
			    (struct pldm_pdr_entity_association
				 *)((uint8_t *)record->data +
				    sizeof(struct pldm_pdr_hdr));
			/*	printf("\npdr->container_id=%d,pdr->num_children=%d",
				       pdr->container_id, pdr->num_children);*/
			struct pldm_entity *child =
			    (struct pldm_entity *)(&pdr->children[0]);
			for (int i = 0; i < pdr->num_children; ++i) {
				if (child->entity_type == entity.entity_type &&
				    child->entity_instance_num ==
					entity.entity_instance_num &&
				    child->entity_container_id ==
					entity.entity_container_id) {
					/*	printf("\nFOUND
					   record_handle=%d",
						       record->record_handle);*/
					found = true;
					record_handle = record->record_handle;
					break;
				}
				child++;
			}
		} // end if PLDM_PDR_ENTITY_ASSOCIATION
		//   record = record->next;
		record = next;
	} // end while
	return record_handle;
}

uint32_t pldm_entity_association_pdr_remove_contained_entity(
    pldm_pdr *repo, pldm_entity entity, uint8_t *event_data_op, bool is_remote)
{
	assert(repo != NULL);
	uint32_t updated_hdl = 0;
	bool removed = false;
	*event_data_op = PLDM_RECORDS_MODIFIED;
	updated_hdl =
	    find_record_handle_by_contained_entity(repo, entity, is_remote);
	if (!updated_hdl) {
		*event_data_op = PLDM_INVALID_OP;
		return updated_hdl;
	}
	/*	printf("\npldm_entity_association_pdr_remove_contained_entity
	   found " "the record handle to delete %d", updated_hdl);*/

	pldm_pdr_record *record = repo->first;
	pldm_pdr_record *prev = repo->first;
	pldm_pdr_record *new_record = malloc(sizeof(pldm_pdr_record));
	new_record->data = NULL; // sm00
	// new_record->data = malloc(record->size - sizeof(pldm_entity)); //sm00
	// new_record->next = NULL; //sm00
	// uint8_t *new_data = new_record->data; //sm00
	while (record != NULL) {
		pldm_pdr_record *next = record->next;
		struct pldm_pdr_hdr *hdr = (struct pldm_pdr_hdr *)record->data;
		if (record->record_handle ==
		    updated_hdl) /*(record->is_remote == is_remote) &&*/
		/*hdr->type == PLDM_PDR_ENTITY_ASSOCIATION)*/ {
			new_record->data =
			    malloc(record->size - sizeof(pldm_entity)); // sm00
			new_record->next = NULL;			// sm00
			new_record->record_handle =
			    htole32(record->record_handle);
			new_record->size =
			    htole32(record->size - sizeof(pldm_entity)); // sm00
			new_record->is_remote = record->is_remote;
			uint8_t *new_start = new_record->data; // sm00 new_data;
			struct pldm_pdr_hdr *new_hdr =
			    (struct pldm_pdr_hdr *)
				new_record->data; // sm00 new_data;
			new_hdr->version = hdr->version;
			new_hdr->record_handle = htole32(hdr->record_handle);
			new_hdr->type = PLDM_PDR_ENTITY_ASSOCIATION;
			new_hdr->record_change_num =
			    htole16(hdr->record_change_num);
			/*new_hdr->length =
			    htole16(record->size - sizeof(struct pldm_pdr_hdr) -
				    sizeof(pldm_entity));*/
			new_hdr->length =
			    htole16(hdr->length - sizeof(pldm_entity)); // sm00
			new_start += sizeof(struct pldm_pdr_hdr);
			struct pldm_pdr_entity_association *new_pdr =
			    (struct pldm_pdr_entity_association *)new_start;

			struct pldm_pdr_entity_association *pdr =
			    (struct pldm_pdr_entity_association
				 *)((uint8_t *)record->data +
				    sizeof(struct pldm_pdr_hdr));
			struct pldm_entity *child =
			    (struct pldm_entity *)(&pdr->children[0]);

			new_pdr->container_id = pdr->container_id;
			new_pdr->association_type = pdr->association_type;
			new_pdr->container.entity_type =
			    pdr->container.entity_type;
			new_pdr->container.entity_instance_num =
			    pdr->container.entity_instance_num;
			new_pdr->container.entity_container_id =
			    pdr->container.entity_container_id;
			new_pdr->num_children =
			    pdr->num_children -
			    1; // if this becomes 0 then just delete. no new
			       // entity assoc pdr is needed PENDING. can test
			       // once pcie cards are placed under slots
			struct pldm_entity *new_child =
			    (struct pldm_entity *)(&new_pdr->children[0]);

			for (int i = 0; i < pdr->num_children; ++i) {
				if (child->entity_type == entity.entity_type &&
				    child->entity_instance_num ==
					entity.entity_instance_num &&
				    child->entity_container_id ==
					entity.entity_container_id) {
					removed = true;
					//	updated_hdl =
					// hdr->record_handle; sm00 not needed
					// as we are getting earlier
					// skip this child.do not add in the
					// new pdr
				} else {
					new_child->entity_type =
					    child->entity_type;
					new_child->entity_instance_num =
					    child->entity_instance_num;
					new_child->entity_container_id =
					    child->entity_container_id;
					new_child++;
				}

				++child;
			}
			if (!new_pdr
				 ->num_children) // record will be deleted and
						 // new_record will not be added
			{
				removed = false;
				*event_data_op = PLDM_RECORDS_DELETED;
				if (repo->first == record) {
					repo->first = record->next;
					record->next = NULL;
				} else if (repo->last == record) {
					repo->last = prev;
					prev->next = NULL;
				} else {
					prev->next = record->next;
					record->next = NULL;
				}
				repo->size -= record->size;
				repo->record_count--;
				if (record->data) {
					free(record->data);
				}
				free(record);
				break;
			} else if (removed) {
				if (repo->first == record) {
					repo->first = new_record;
					new_record->next = record->next;
				} else {
					prev->next = new_record;
					new_record->next = record->next;
					record->next = NULL; // sm00
				}
				if (repo->last == record) {
					repo->last = new_record;
					new_record->next = NULL; // sm00
				}
				repo->size -= record->size;
				repo->size += new_record->size;

				if (record->data) {
					free(record->data);
				}
				free(record);
				break;
			}
		}
		prev = record;
		record = next;
	}
	if (!removed) {
		if (new_record->data) // sm00
		{
			free(new_record->data); // sm00
		}
		if (new_record) {
			free(new_record);
		}
		//	free(new_data); sm00
	}
	return updated_hdl;
}

uint32_t pldm_entity_association_pdr_add_contained_entity(
    pldm_pdr *repo, pldm_entity entity, pldm_entity parent,
    uint8_t *event_data_op, bool is_remote)
{
	/*	printf("\nenter pldm_entity_association_pdr_add_contained_entity
	   " "entity type=%d entity ins=%d container id=%d", entity.entity_type,
	   entity.entity_instance_num, entity.entity_container_id); printf("\n
	   and parent entity type=%d entity ins=%d container id=%d",
		       parent.entity_type, parent.entity_instance_num,
		       parent.entity_container_id);*/
	// testing pending with pcie slot-card. can test once cards are placed
	// under slots in DBus. usecase: will not find the PDR and need to
	// create a new entity assoc PDR since the number of child is always 1
	// if the PDR is not found then search for the PDR having parent as a
	// child if found then parent is valid and create a new enitity assoc
	// PDR with parent-entity
	uint32_t updated_hdl = 0;
	bool added = false;
	*event_data_op = PLDM_RECORDS_MODIFIED;
	pldm_pdr_record *record = repo->first;
	pldm_pdr_record *prev = repo->first;
	pldm_pdr_record *new_record = malloc(sizeof(pldm_pdr_record));
	new_record->data = NULL; // sm00
	// new_record->data = malloc(record->size + sizeof(pldm_entity)); //sm00
	// new_record->next = NULL; //sm00
	uint8_t *new_data = NULL; // new_record->data; //sm00
	bool found = false;
	while (record != NULL) {
		pldm_pdr_record *next = record->next;
		struct pldm_pdr_hdr *hdr = (struct pldm_pdr_hdr *)record->data;
		if ((record->is_remote == is_remote) &&
		    hdr->type == PLDM_PDR_ENTITY_ASSOCIATION) {
			struct pldm_pdr_entity_association *pdr =
			    (struct pldm_pdr_entity_association
				 *)((uint8_t *)record->data +
				    sizeof(struct pldm_pdr_hdr));
			if (pdr->container.entity_type == parent.entity_type &&
			    pdr->container.entity_instance_num ==
				parent.entity_instance_num &&
			    pdr->container.entity_container_id ==
				parent.entity_container_id) {
				found = true;
				new_record->data = malloc(
				    record->size + sizeof(pldm_entity)); // sm00
				new_record->next = NULL;		 // sm00
				new_data = new_record->data;		 // sm00
				updated_hdl = record->record_handle;
				new_record->record_handle =
				    htole32(record->record_handle);
				new_record->size =
				    htole32(record->size + sizeof(pldm_entity));
				new_record->is_remote = record->is_remote;
				uint8_t *new_start = new_data;
				struct pldm_pdr_hdr *new_hdr =
				    (struct pldm_pdr_hdr *)new_data;
				new_hdr->version = hdr->version;
				new_hdr->record_handle =
				    htole32(hdr->record_handle);
				new_hdr->type = PLDM_PDR_ENTITY_ASSOCIATION;
				new_hdr->record_change_num =
				    htole16(hdr->record_change_num);
				/*	new_hdr->length = htole16(
					    record->size - sizeof(struct
				   pldm_pdr_hdr) + sizeof(pldm_entity)); */
				new_hdr->length = htole16(
				    hdr->length + sizeof(pldm_entity)); // sm00
				new_start += sizeof(struct pldm_pdr_hdr);
				struct pldm_pdr_entity_association *new_pdr =
				    (struct pldm_pdr_entity_association *)
					new_start;

				struct pldm_entity *child =
				    (struct pldm_entity *)(&pdr->children[0]);

				new_pdr->container_id = pdr->container_id;
				new_pdr->association_type =
				    pdr->association_type;
				new_pdr->container.entity_type =
				    pdr->container.entity_type;
				new_pdr->container.entity_instance_num =
				    pdr->container.entity_instance_num;
				new_pdr->container.entity_container_id =
				    pdr->container.entity_container_id;
				new_pdr->num_children = pdr->num_children + 1;
				struct pldm_entity *new_child =
				    (struct pldm_entity *)(&new_pdr
								->children[0]);
				for (int i = 0; i < pdr->num_children; ++i) {
					new_child->entity_type =
					    child->entity_type;
					new_child->entity_instance_num =
					    child->entity_instance_num;
					new_child->entity_container_id =
					    child->entity_container_id;
					new_child++;
					child++;
				}
				new_child->entity_type = entity.entity_type;
				new_child->entity_instance_num =
				    entity.entity_instance_num;
				new_child->entity_container_id =
				    entity.entity_container_id;

				added = true;
				if (repo->first == record) {
					repo->first = new_record;
					new_record->next = record->next;
				} else {
					prev->next = new_record;
					new_record->next = record->next;
				}
				if (repo->last == record) {
					repo->last = new_record;
				}
				repo->size -= record->size;
				repo->size += new_record->size;

				if (record->data) {
					free(record->data);
				}
				free(record);
				break;
			}
		}

		prev = record;
		record = next;
	}
	if (!found && !is_remote) // need to create a new entity assoc pdr
	{
		//	printf("\ncreating a new entity assoc pdr for slot \n");
		uint8_t num_children = 1;
		added = true;
		*event_data_op = PLDM_RECORDS_ADDED;
		prev = repo->first;
		pldm_pdr_record *curr = repo->first;
		while (curr != NULL) {
			if (!prev->is_remote && curr->is_remote) {
				// printf("\nfound the place \n");
				break;
			}
			prev = curr;
			curr = curr->next;
		}

		uint16_t new_pdr_size = sizeof(struct pldm_pdr_hdr) +
					sizeof(uint16_t) + sizeof(uint8_t) +
					sizeof(pldm_entity) + sizeof(uint8_t) +
					num_children * sizeof(pldm_entity);
		new_record->data = malloc(new_pdr_size);
		new_record->record_handle = prev->record_handle + 1;
		new_record->size = new_pdr_size;
		new_record->is_remote = false;
		new_record->next = prev->next;
		prev->next = new_record;
		if (repo->last == prev) {
			repo->last = new_record;
		}
		repo->size += new_record->size;
		++repo->record_count;

		updated_hdl = new_record->record_handle;

		struct pldm_pdr_hdr *new_hdr =
		    (struct pldm_pdr_hdr *)new_record->data;
		new_hdr->version = 1;
		new_hdr->record_handle = new_record->record_handle;
		new_hdr->type = PLDM_PDR_ENTITY_ASSOCIATION;
		new_hdr->record_change_num = 0;
		new_hdr->length =
		    htole16(new_pdr_size - sizeof(struct pldm_pdr_hdr));

		struct pldm_pdr_entity_association *new_pdr =
		    (struct pldm_pdr_entity_association
			 *)((uint8_t *)new_record->data +
			    sizeof(struct pldm_pdr_hdr));
		new_pdr->container.entity_type = parent.entity_type;
		new_pdr->container.entity_instance_num =
		    parent.entity_instance_num;
		new_pdr->container.entity_container_id =
		    parent.entity_container_id;
		new_pdr->container_id = entity.entity_container_id;
		new_pdr->association_type = PLDM_ENTITY_ASSOCIAION_PHYSICAL;
		new_pdr->num_children = 1;
		struct pldm_entity *new_child =
		    (struct pldm_entity *)(&new_pdr->children[0]);
		new_child->entity_type = entity.entity_type;
		new_child->entity_instance_num = entity.entity_instance_num;
		new_child->entity_container_id = entity.entity_container_id;
	}
	if (!added) {
		if (new_record->data) // sm00
		{
			free(new_record->data); // sm00
		}
		if (new_record) // sm00
		{
			free(new_record); // sm00
		}
	}
	//	printf("\nreturning updated_hdl=%d", updated_hdl);
	return updated_hdl;
}

void find_entity_ref_in_tree(pldm_entity_node *tree_node, pldm_entity entity,
			     pldm_entity_node **node)
{
	if (tree_node == NULL) {
		return;
	}

	if (tree_node->entity.entity_type == entity.entity_type &&
	    tree_node->entity.entity_instance_num ==
		entity.entity_instance_num &&
	    tree_node->entity.entity_container_id ==
		entity.entity_container_id) {

		*node = tree_node;
		return;
	}

	find_entity_ref_in_tree(tree_node->first_child, entity, node);
	find_entity_ref_in_tree(tree_node->next_sibling, entity, node);
}

void pldm_find_entity_ref_in_tree(pldm_entity_association_tree *tree,
				  pldm_entity entity, pldm_entity_node **node)
{
	assert(tree != NULL);
	find_entity_ref_in_tree(tree->root, entity, node);
}

void pldm_pdr_remove_pdrs_by_terminus_handle(uint32_t terminus_handle,
					     pldm_pdr *repo)
{
	assert(repo != NULL);
	bool removed = false;

	pldm_pdr_record *record = repo->first;
	pldm_pdr_record *prev = NULL;
	while (record != NULL) {
		pldm_pdr_record *next = record->next;
		if (record->terminus_handle == terminus_handle) {
			if (repo->first == record) {
				repo->first = next;
			} else {
				prev->next = next;
			}
			if (repo->last == record) {
				repo->last = prev;
			}
			if (record->data) {
				free(record->data);
			}
			--repo->record_count;
			repo->size -= record->size;
			free(record);
			removed = true;
		} else {
			prev = record;
		}
		record = next;
	}

	if (removed == true) {
		record = repo->first;
		uint32_t record_handle = 0;
		while (record != NULL) {
			record->record_handle = ++record_handle;
			if (record->data != NULL) {
				struct pldm_pdr_hdr *hdr =
				    (struct pldm_pdr_hdr *)(record->data);
				hdr->record_handle =
				    htole32(record->record_handle);
			}
			record = record->next;
		}
	}
}
void pldm_pdr_remove_remote_pdrs(pldm_pdr *repo)
{
	assert(repo != NULL);
	bool removed = false;

	pldm_pdr_record *record = repo->first;
	pldm_pdr_record *prev = NULL;
	while (record != NULL) {
		pldm_pdr_record *next = record->next;
		if (record->is_remote == true) {
			if (repo->first == record) {
				repo->first = next;
			} else {
				prev->next = next;
			}
			if (repo->last == record) {
				repo->last = prev;
			}
			if (record->data) {
				free(record->data);
			}
			--repo->record_count;
			repo->size -= record->size;
			free(record);
			removed = true;
		} else {
			prev = record;
		}
		record = next;
	}

	if (removed == true) {
		record = repo->first;
		uint32_t record_handle = 0;
		while (record != NULL) {
			record->record_handle = ++record_handle;
			if (record->data != NULL) {
				struct pldm_pdr_hdr *hdr =
				    (struct pldm_pdr_hdr *)(record->data);
				hdr->record_handle =
				    htole32(record->record_handle);
			}
			record = record->next;
		}
	}
}

void entity_association_tree_find(pldm_entity_node *node, pldm_entity *entity,
				  pldm_entity_node **out, bool is_remote)
{
	if (node == NULL) {
		return;
	}
	if (is_remote) {

		if (node->entity.entity_type == entity->entity_type &&
		    node->entity.entity_instance_num ==
			entity->entity_instance_num &&
		    node->host_container_id == entity->entity_container_id) {
			entity->entity_container_id =
			    node->entity.entity_container_id;

			*out = node;
			return;
		}
	} else {
		if (node->entity.entity_type == entity->entity_type &&
		    node->entity.entity_instance_num ==
			entity->entity_instance_num) {
			entity->entity_container_id =
			    node->entity.entity_container_id;
			*out = node;
			return;
		}
	}

	entity_association_tree_find(node->next_sibling, entity, out,
				     is_remote);
	entity_association_tree_find(node->first_child, entity, out, is_remote);
}

pldm_entity_node *
pldm_entity_association_tree_find(pldm_entity_association_tree *tree,
				  pldm_entity *entity, bool is_remote)
{
	assert(tree != NULL);

	pldm_entity_node *node = NULL;
	entity_association_tree_find(tree->root, entity, &node, is_remote);
	return node;
}

static void entity_association_tree_copy(pldm_entity_node *org_node,
					 pldm_entity_node **new_node)
{
	if (org_node == NULL) {
		return;
	}
	*new_node = malloc(sizeof(pldm_entity_node));
	(*new_node)->parent = org_node->parent;
	(*new_node)->entity = org_node->entity;
	(*new_node)->association_type = org_node->association_type;
	(*new_node)->host_container_id = org_node->host_container_id;
	(*new_node)->first_child = NULL;
	(*new_node)->next_sibling = NULL;
	entity_association_tree_copy(org_node->first_child,
				     &((*new_node)->first_child));
	entity_association_tree_copy(org_node->next_sibling,
				     &((*new_node)->next_sibling));
}

void pldm_entity_association_tree_copy_root(
    pldm_entity_association_tree *org_tree,
    pldm_entity_association_tree *new_tree)
{
	new_tree->last_used_container_id = org_tree->last_used_container_id;
	entity_association_tree_copy(org_tree->root, &(new_tree->root));
}

void pldm_entity_association_tree_destroy_root(
    pldm_entity_association_tree *tree)
{
	assert(tree != NULL);
	entity_association_tree_destroy(tree->root);
	tree->last_used_container_id = 0;
	tree->root = NULL;
}

bool pldm_is_empty_entity_assoc_tree(pldm_entity_association_tree *tree)
{
	return ((tree->root == NULL) ? true : false);
}

void pldm_entity_association_pdr_extract(const uint8_t *pdr, uint16_t pdr_len,
					 size_t *num_entities,
					 pldm_entity **entities)
{
	assert(pdr != NULL);
	assert(pdr_len >= sizeof(struct pldm_pdr_hdr) +
			      sizeof(struct pldm_pdr_entity_association));

	struct pldm_pdr_hdr *hdr = (struct pldm_pdr_hdr *)pdr;
	assert(hdr->type == PLDM_PDR_ENTITY_ASSOCIATION);

	const uint8_t *start = (uint8_t *)pdr;
	const uint8_t *end =
	    start + sizeof(struct pldm_pdr_hdr) + le16toh(hdr->length);
	start += sizeof(struct pldm_pdr_hdr);
	struct pldm_pdr_entity_association *entity_association_pdr =
	    (struct pldm_pdr_entity_association *)start;
	*num_entities = entity_association_pdr->num_children + 1;
	assert(*num_entities >= 2);
	*entities = malloc(sizeof(pldm_entity) * *num_entities);
	assert(*entities != NULL);
	assert(start + sizeof(struct pldm_pdr_entity_association) +
		   sizeof(pldm_entity) * (*num_entities - 2) ==
	       end);
	(*entities)->entity_type =
	    le16toh(entity_association_pdr->container.entity_type);
	(*entities)->entity_instance_num =
	    le16toh(entity_association_pdr->container.entity_instance_num);
	(*entities)->entity_container_id =
	    le16toh(entity_association_pdr->container.entity_container_id);
	pldm_entity *curr_entity = entity_association_pdr->children;
	size_t i = 1;
	while (i < *num_entities) {
		(*entities + i)->entity_type =
		    le16toh(curr_entity->entity_type);
		(*entities + i)->entity_instance_num =
		    le16toh(curr_entity->entity_instance_num);
		(*entities + i)->entity_container_id =
		    le16toh(curr_entity->entity_container_id);
		++curr_entity;
		++i;
	}
}
