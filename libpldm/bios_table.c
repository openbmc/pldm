#include <assert.h>
#include <endian.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bios.h"
#include "bios_table.h"

uint8_t pldm_bios_table_attr_entry_enum_decode_pv_num(
    const struct pldm_bios_attr_table_entry *entry)
{
	return entry->metadata[0];
}

uint8_t pldm_bios_table_attr_entry_enum_decode_def_num(
    const struct pldm_bios_attr_table_entry *entry)
{
	uint8_t pv_num = pldm_bios_table_attr_entry_enum_decode_pv_num(entry);
	return entry->metadata[sizeof(uint8_t) /* pv_num */ +
			       sizeof(uint16_t) * pv_num];
}

int pldm_bios_table_attr_entry_enum_decode_pv_hdls(
    const struct pldm_bios_attr_table_entry *entry, uint16_t *pv_hdls,
    uint8_t pv_num)
{
	uint8_t num = pldm_bios_table_attr_entry_enum_decode_pv_num(entry);
	assert(num == pv_num);
	size_t i;
	for (i = 0; i < pv_num; i++) {
		uint16_t *hdl = (uint16_t *)(entry->metadata + sizeof(uint8_t) +
					     i * sizeof(uint16_t));
		pv_hdls[i] = le16toh(*hdl);
	}
	return PLDM_SUCCESS;
}

/** @brief Get length of an enum attribute entry
 */
static size_t
attr_table_entry_length_enum(const struct pldm_bios_attr_table_entry *entry)
{
	uint8_t pv_num = pldm_bios_table_attr_entry_enum_decode_pv_num(entry);
	uint8_t def_num = pldm_bios_table_attr_entry_enum_decode_def_num(entry);
	return sizeof(*entry) - 1 + sizeof(pv_num) + pv_num * sizeof(uint16_t) +
	       sizeof(def_num) + def_num;
}

#define ATTR_ENTRY_STRING_LENGTH_MIN                                           \
	sizeof(uint8_t) /* string type */ +                                    \
	    sizeof(uint16_t) /* minimum string length */ +                     \
	    sizeof(uint16_t) /* maximum string length */ +                     \
	    sizeof(uint16_t) /* default string length */

uint16_t pldm_bios_table_attr_entry_string_decode_def_string_length(
    const struct pldm_bios_attr_table_entry *entry)
{
	int def_string_pos = ATTR_ENTRY_STRING_LENGTH_MIN - sizeof(uint16_t);
	uint16_t def_string_length =
	    *(uint16_t *)(entry->metadata + def_string_pos);
	return le16toh(def_string_length);
}

/** @brief Get length of a string attribute entry
 */
static size_t
attr_table_entry_length_string(const struct pldm_bios_attr_table_entry *entry)
{
	uint16_t def_string_len =
	    pldm_bios_table_attr_entry_string_decode_def_string_length(entry);
	return sizeof(*entry) - 1 + ATTR_ENTRY_STRING_LENGTH_MIN +
	       def_string_len;
}

struct attr_table_entry {
	uint8_t attr_type;
	size_t (*entry_length_handler)(
	    const struct pldm_bios_attr_table_entry *);
};

static struct attr_table_entry attr_table_entrys[] = {
    {.attr_type = PLDM_BIOS_ENUMERATION,
     .entry_length_handler = attr_table_entry_length_enum},
    {.attr_type = PLDM_BIOS_ENUMERATION_READ_ONLY,
     .entry_length_handler = attr_table_entry_length_enum},
    {.attr_type = PLDM_BIOS_STRING,
     .entry_length_handler = attr_table_entry_length_string},
    {.attr_type = PLDM_BIOS_STRING_READ_ONLY,
     .entry_length_handler = attr_table_entry_length_string},
};

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

static struct attr_table_entry *find_attr_table_entry_by_type(uint8_t attr_type)
{
	size_t i;
	for (i = 0; i < ARRAY_SIZE(attr_table_entrys); i++) {
		if (attr_type == attr_table_entrys[i].attr_type)
			return &attr_table_entrys[i];
	}
	return NULL;
}

static size_t attr_table_entry_length(const void *table_entry)
{
	const struct pldm_bios_attr_table_entry *entry = table_entry;
	struct attr_table_entry *attr_table_entry =
	    find_attr_table_entry_by_type(entry->attr_type);
	assert(attr_table_entry != NULL);
	assert(attr_table_entry->entry_length_handler != NULL);

	return attr_table_entry->entry_length_handler(entry);
}

size_t pldm_bios_table_attr_value_entry_encode_enum_length(uint8_t count)
{
	size_t length =
	    sizeof(uint16_t) + sizeof(uint8_t) + sizeof(count) + count;
	return length;
}

int pldm_bios_table_attr_value_entry_encode_enum(
    void *entry, size_t entry_length, uint16_t attr_handle, uint8_t attr_type,
    uint8_t count, uint8_t *handles)
{
	size_t length =
	    pldm_bios_table_attr_value_entry_encode_enum_length(count);
	assert(length <= entry_length);

	struct pldm_bios_attr_val_table_entry *table_entry = entry;
	table_entry->attr_handle = htole16(attr_handle);
	table_entry->attr_type = attr_type;
	table_entry->value[0] = count;
	memcpy(&table_entry->value[1], handles, count);
	return PLDM_SUCCESS;
}

size_t
pldm_bios_table_attr_value_entry_encode_string_length(uint16_t string_length)
{
	size_t length = sizeof(uint16_t) + sizeof(uint8_t) +
			sizeof(string_length) + string_length;
	return length;
}

int pldm_bios_table_attr_value_entry_encode_string(
    void *entry, size_t entry_length, uint16_t attr_handle, uint8_t attr_type,
    uint16_t str_length, const char *str)
{
	size_t length =
	    pldm_bios_table_attr_value_entry_encode_string_length(str_length);
	assert(length <= entry_length);

	struct pldm_bios_attr_val_table_entry *table_entry = entry;
	table_entry->attr_handle = htole16(attr_handle);
	table_entry->attr_type = attr_type;
	memcpy(table_entry->value + sizeof(str_length), str, str_length);
	str_length = htole16(str_length);
	memcpy(table_entry->value, &str_length, sizeof(str_length));
	return PLDM_SUCCESS;
}

struct pldm_bios_table_iter {
	const uint8_t *table_data;
	size_t table_len;
	size_t next_pos;
	size_t (*entry_length_handler)(const void *table_entry);
};

struct pldm_bios_table_iter *
pldm_bios_table_iter_create(const void *table, const size_t length,
			    enum pldm_bios_table_types type)
{
	struct pldm_bios_table_iter *iter = malloc(sizeof(*iter));
	assert(iter != NULL);
	iter->table_data = table;
	iter->table_len = length;
	iter->next_pos = 0;
	switch (type) {
	case PLDM_BIOS_STRING_TABLE:
		iter->entry_length_handler = NULL;
		break;
	case PLDM_BIOS_ATTR_TABLE:
		iter->entry_length_handler = attr_table_entry_length;
		break;
	case PLDM_BIOS_ATTR_VAL_TABLE:
		iter->entry_length_handler = NULL;
		break;
	}

	return iter;
}

void pldm_bios_table_iter_free(struct pldm_bios_table_iter *iter)
{
	free(iter);
}

#define pad_and_check_max 7
bool pldm_bios_table_iter_is_end(const struct pldm_bios_table_iter *iter)
{
	if (iter->table_len - iter->next_pos <= pad_and_check_max)
		return true;
	return false;
}

const void *pldm_bios_table_entry_next(struct pldm_bios_table_iter *iter)
{
	if (pldm_bios_table_iter_is_end(iter))
		return NULL;
	const void *entry = iter->table_data + iter->next_pos;
	iter->next_pos += iter->entry_length_handler(entry);

	return entry;
}
