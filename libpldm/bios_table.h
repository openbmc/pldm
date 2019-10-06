#ifndef BIOS_TABLE_H__
#define BIOS_TABLE_H__

#ifdef __cplusplus
extern "C" {
#endif

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
 *  @return Iterator to the beginning
 */
struct pldm_bios_table_iter *pldm_bios_table_iter_create(const uint8_t *table,
							 size_t length);

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
 *  @details Return pointer to entry that the iterator current point to and get
 *  the iterator to next entry
 *  @param[in] iter - Pointer the bios attribute table iterator
 *  @return Pointer to an entry in bios attribute table
 */
const struct pldm_bios_attr_table_entry *
pldm_bios_table_attr_entry_next(struct pldm_bios_table_iter *iter);

/** @brief Get the total number of possible values for the entry
 *  @param[in] entry - Pointer to bios attribute table entry
 *  @return total number of possible values
 */
uint8_t pldm_bios_table_attr_entry_enum_decode_pv_num(
    const struct pldm_bios_attr_table_entry *entry);

/** @brief Get the total number of default values for the entry
 *  @param[in] entry - Pointer to bios attribute table entry
 *  @return total number of default values
 */
uint8_t pldm_bios_table_attr_entry_enum_decode_def_num(
    const struct pldm_bios_attr_table_entry *entry);

/** @brief Get the length of default string in bytes for the entry
 *  @param[in] entry - Pointer to bios attribute table entry
 *  @return length of default string in bytes
 */
uint16_t pldm_bios_table_attr_entry_string_decode_def_string_length(
    const struct pldm_bios_attr_table_entry *entry);

/** @brief Get possible values string handles
 *  @param[in] entry - Pointer to bios attribute table entry
 *  @param[out] pv_hdls - Pointer to PossibleValuesStringHandles
 *  @param[in] pv_num - number of PossibleValuesStirngHandles
 *  @return pldm_completion_codes
 */
int pldm_bios_table_attr_entry_enum_decode_pv_hdls(
    const struct pldm_bios_attr_table_entry *entry, uint16_t *pv_hdls,
    uint8_t pv_num);

/** @brief Get length that an attribute value entry(type: enum) will take
 *  @param[in] count - Total number of current values for this enumeration
 *  @return The length that an entry(type: enum) will take
 */
size_t pldm_bios_table_attr_value_entry_encode_enum_length(uint8_t count);

/** @brief Create an attribute value entry(type: enum)
 *  @param[out] entry - Pointer to bios attribute value entry
 *  @param[in] entry_length - Length of attribute value entry
 *  @param[in] attr_handle - This handle points to an attribute in the
 *  BIOS Attribute Vlaue Table.
 *  @param[in] attr_type - Type of this attribute in the BIOS Attribute Value
 * Table
 *  @param[in] count - Total number of current values for this enum attribute
 *  @param[in] handle_indexes - Index into the array(provided in the BIOS
 * Attribute Table) of the possible values of string handles for this attribute.
 *  @return pldm_completion_codes
 */
int pldm_bios_table_attr_value_entry_encode_enum(
    void *entry, size_t entry_length, uint16_t attr_handle, uint8_t attr_type,
    uint8_t count, uint8_t *handle_indexes);

/** @brief Get length that an attribute value entry(type: string) will take
 *  @param[in] string_length - Length of the current string in byte, 0 indicates
 *  that the current string value is not set.
 *  @return The length that an entry(type: string) will take
 */
size_t
pldm_bios_table_attr_value_entry_encode_string_length(uint16_t string_length);

/** @brief Create an attribute value entry(type: string)
 *  @param[in] entry - Pointer to bios attribute value entry
 *  @param[in] entry_length - Length of attribute value entry
 *  @param[in] attr_handle - This handle points to an attribute in the
 *  BIOS Attribute Vlaue Table.
 *  @param[in] attr_type - Type of this attribute in the BIOS Attribute Value
 * Table
 *  @param[in] string_length - Length of current string in bytes. 0 indicates
 * that the current string value is not set.
 *  @param[in] string - The current string itsel
 *  @return pldm_completion_codes
 */
int pldm_bios_table_attr_value_entry_encode_string(
    void *entry, size_t entry_length, uint16_t attr_handle, uint8_t attr_type,
    uint16_t string_length, const char *string);

#ifdef __cplusplus
}
#endif

#endif
