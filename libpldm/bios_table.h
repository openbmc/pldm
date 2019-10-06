#ifndef BIOS_TABLE_H__
#define BIOS_TABLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bios.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @struct pldm_bios_table_iter
 *  structure representing bios table iterator
 */
struct pldm_bios_table_iter;

/** @brief Create a bios table iterator
 *  @param[in] table - Pointer to table data
 *  @param[in] length - Length of table data
 *  @param[in] type - Type of pldm bios table
 *  @return Iterator to the beginning
 */
struct pldm_bios_table_iter *
pldm_bios_table_iter_create(const void *table, size_t length,
			    enum pldm_bios_table_types type);

/** @brief Release a bios table iterator
 *  @param[in] iter - Pointer to bios table iterator
 */
void pldm_bios_table_iter_free(struct pldm_bios_table_iter *iter);

/** @brief Check if the iterator reaches the end of the bios table
 *  @param[in] iter - Pointer to the bios table iterator
 *  @return true if iterator reaches the end
 *  @note *end* is a position after the last entry.
 */
bool pldm_bios_table_iter_is_end(const struct pldm_bios_table_iter *iter);

/** @brief Get iterator to next entry
 *  @param[in] iter - Pointer the bios table iterator
 */
void pldm_bios_table_iter_next(struct pldm_bios_table_iter *iter);

/** @brief Get the bios table entry that the iterator points to
 *  @param[in] iter - Pointer to the bios table iterator
 *  @return Pointer to an entry in bios table
 */
const void *pldm_bios_table_iter_value(struct pldm_bios_table_iter *iter);

/** @brief Get the bios attribute table entry that the iterator points to
 *  @param[in] iter - Pointer the bios attribute table iterator
 *  @return Pointer to an entry in bios attribute table
 */
static inline const struct pldm_bios_attr_table_entry *
pldm_bios_table_iter_attr_entry_value(struct pldm_bios_table_iter *iter)
{
	return (const struct pldm_bios_attr_table_entry *)
	    pldm_bios_table_iter_value(iter);
}

/** @brief Get the total number of possible values for the entry
 *  @param[in] entry - Pointer to bios attribute table entry
 *  @return total number of possible values
 */
uint8_t pldm_bios_table_attr_entry_enum_decode_pv_num(
    const struct pldm_bios_attr_table_entry *entry);

/** @brief Get the total number of possible values for the entry and check the
 * validity of the parameters
 *  @param[in] entry - Pointer to bios attribute table entry
 *  @param[out] pv_num - Pointer to total number of possible values
 *  @return pldm_completion_codes
 */
int pldm_bios_table_attr_entry_enum_decode_pv_num_check(
    const struct pldm_bios_attr_table_entry *entry, uint8_t *pv_num);

/** @brief Get the total number of default values for the entry
 *  @param[in] entry - Pointer to bios attribute table entry
 *  @return total number of default values
 */
uint8_t pldm_bios_table_attr_entry_enum_decode_def_num(
    const struct pldm_bios_attr_table_entry *entry);

/** @brief Get the total number of default values for the entry and check the
 * validity of the parameters
 *  @param[in] entry - Pointer to bios attribute table entry
 *  @param[out] def_num - Pointer to total number of default values
 *  @return pldm_completion_codes
 */
int pldm_bios_table_attr_entry_enum_decode_def_num_check(
    const struct pldm_bios_attr_table_entry *entry, uint8_t *def_num);

/** @brief Get the length of default string in bytes for the entry
 *  @param[in] entry - Pointer to bios attribute table entry
 *  @return length of default string in bytes
 */
uint16_t pldm_bios_table_attr_entry_string_decode_def_string_length(
    const struct pldm_bios_attr_table_entry *entry);

/** @brief Get the length of default string in bytes for the entry and check the
 * validity of the parameters
 *  @param[in] entry - Pointer to bios attribute table entry
 *  @param[out] def_string_length Pointer to length of default string in bytes
 *  @return pldm_completion_codes
 */
int pldm_bios_table_attr_entry_string_decode_def_string_length_check(
    const struct pldm_bios_attr_table_entry *entry,
    uint16_t *def_string_length);

#ifdef __cplusplus
}
#endif

#endif
