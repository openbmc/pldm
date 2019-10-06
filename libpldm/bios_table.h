#ifndef BIOS_TABLE_H__
#define BIOS_TABLE_H__
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct pldm_bios_table_attr_iter;

struct pldm_bios_table_attr_iter *
pldm_bios_table_attr_iter_create(uint8_t *table, size_t length);

void pldm_bios_table_attr_iter_free(struct pldm_bios_table_attr_iter *iter);

bool pldm_bios_table_attr_iter_is_end(struct pldm_bios_table_attr_iter *iter);

const struct pldm_bios_attr_table_entry *
pldm_bios_table_attr_entry_next(struct pldm_bios_table_attr_iter *iter);

#ifdef __cplusplus
}
#endif
#endif