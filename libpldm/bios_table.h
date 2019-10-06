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

int pldm_bios_table_attr_entry_enum_decode_pv_num(
    const struct pldm_bios_attr_table_entry *entry, uint8_t *pv_num);

int pldm_bios_table_attr_entry_enum_decode_pv_hdls(
    const struct pldm_bios_attr_table_entry *entry, uint16_t *pv_hdls,
    uint8_t pv_num);

size_t pldm_bios_table_attr_vallue_entry_encode_enum_length(uint8_t count);

int pldm_bios_table_attr_value_entry_encode_enum(
    void *table, size_t table_length, uint16_t attr_handle, uint8_t attr_type,
    uint8_t count, uint8_t *handles);

size_t
pldm_bios_table_attr_vallue_entry_encode_string_length(uint16_t string_length);
int pldm_bios_table_attr_vallue_entry_encode_string(
    void *table, size_t table_length, uint16_t attr_handle, uint8_t attr_type,
    uint16_t string_length, const char *string);

#ifdef __cplusplus
}
#endif
#endif