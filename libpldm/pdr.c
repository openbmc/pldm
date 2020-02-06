#include "pdr.h"
#include "platform.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct pldm_pdr_record {
	uint32_t record_handle;
	uint32_t size;
	uint8_t *data;
	struct pldm_pdr_record *next;
} pldm_pdr_record;

typedef struct pldm_pdr {
	uint32_t record_count;
	uint32_t size;
	uint32_t last_used_record_handle;
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
	repo->last_used_record_handle = record->record_handle;
	++repo->record_count;
}

static inline uint32_t get_new_record_handle(const pldm_pdr *repo)
{
	assert(repo != NULL);
	assert(repo->last_used_record_handle != UINT32_MAX);

	return repo->last_used_record_handle + 1;
}

static pldm_pdr_record *make_new_record(const pldm_pdr *repo,
					const uint8_t *data, uint32_t size,
					uint32_t record_handle)
{
	assert(repo != NULL);
	assert(size != 0);

	pldm_pdr_record *record = malloc(sizeof(pldm_pdr_record));
	assert(record != NULL);
	record->record_handle =
	    record_handle == 0 ? get_new_record_handle(repo) : record_handle;
	record->size = size;
	if (data != NULL) {
		record->data = malloc(size);
		assert(record->data != NULL);
		memcpy(record->data, data, size);
		if (!record_handle) {
			struct pldm_pdr_hdr *hdr =
			    (struct pldm_pdr_hdr *)(record->data);
			hdr->record_handle = record->record_handle;
		}
	}
	record->next = NULL;

	return record;
}

uint32_t pldm_pdr_add(pldm_pdr *repo, const uint8_t *data, uint32_t size,
		      uint32_t record_handle)
{
	assert(size != 0);
	assert(data != NULL);

	pldm_pdr_record *record =
	    make_new_record(repo, data, size, record_handle);
	add_record(repo, record);

	return record->record_handle;
}

pldm_pdr *pldm_pdr_init()
{
	pldm_pdr *repo = malloc(sizeof(pldm_pdr));
	assert(repo != NULL);
	repo->record_count = 0;
	repo->size = 0;
	repo->last_used_record_handle = 0;
	repo->first = NULL;
	repo->last = NULL;

	return repo;
}

void pldm_pdr_destroy(pldm_pdr **repo)
{
	assert(repo != NULL);
	assert(*repo != NULL);

	pldm_pdr_record *record = (*repo)->first;
	while (record != NULL) {
		pldm_pdr_record *next = record->next;
		if (record->data) {
			free(record->data);
			record->data = NULL;
		}
		free(record);
		record = next;
	}
	free(*repo);
	*repo = NULL;
}

const pldm_pdr_record *pldm_pdr_find_record(const pldm_pdr *repo,
					    uint32_t record_handle,
					    uint8_t **data, uint32_t *size,
					    uint32_t *next_record_handle)
{
	assert(repo != NULL);
	assert(data != NULL);
	assert(*data == NULL);
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

const pldm_pdr_record *
pldm_pdr_get_next_record(const pldm_pdr *repo,
			 const pldm_pdr_record *curr_record, uint8_t **data,
			 uint32_t *size, uint32_t *next_record_handle)
{
	assert(repo != NULL);
	assert(curr_record != NULL);
	assert(data != NULL);
	assert(*data == NULL);
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
	assert(data != NULL);
	assert(*data == NULL);
	assert(size != NULL);

	pldm_pdr_record *record = repo->first;
	if (curr_record != NULL) {
		record = curr_record->next;
	}
	while (record != NULL) {
		struct pldm_pdr_hdr *hdr = (struct pldm_pdr_hdr *)record->data;
		if (hdr->type == pdr_type) {
			*size = record->size;
			*data = record->data;
			return record;
		}
		record = record->next;
	}

	*size = 0;
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
