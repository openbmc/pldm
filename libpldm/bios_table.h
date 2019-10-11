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

/** @brief Get the bios string table entry that the iterator ponit to
 *  @param[in] iter - Pointer the bios string table iterator
 *  @return Pointer to an entry in bios string table
 */
static inline const struct pldm_bios_string_table_entry *
pldm_bios_table_iter_string_entry_value(struct pldm_bios_table_iter *iter)
{
	return (const struct pldm_bios_string_table_entry *)
	    pldm_bios_table_iter_value(iter);
}

/** @brief Get the length of an entry in the BIOS String Table
 *  @param[in] string_length - Length of string
 *  @return Length of an entry in bytes
 */
size_t pldm_bios_table_string_entry_encode_length(uint16_t string_length);

/** @brief Create an entry of BIOS String Table
 *  @param[out] entry - Pointer to a buffer to create an entry
 *  @param[in] entry_length - Length of the buffer to create an entry
 *  @param[in] str - String itself
 *  @param[in] str_length - Length of the string
 */
void pldm_bios_table_string_entry_encode(void *entry, size_t entry_length,
					 const char *str, uint16_t str_length);

/** @brief Create an entry of BIOS String Table and check the validity of the
 * parameters
 *  @param[out] entry - Pointer to a buffer to create an entry
 *  @param[in] entry_length - Length of the buffer to create an entry
 *  @param[in] str - String itself
 *  @param[in] str_length - Length of the string
 *  @return pldm_completion_codes
 */
int pldm_bios_table_string_entry_encode_check(void *entry, size_t entry_length,
					      const char *str,
					      uint16_t str_length);

/** @brief Get the string handle for the entry
 *  @param[in] entry - Pointer to a bios string table entry
 *  @return Handle to identify a string in the bios string table
 */
uint16_t pldm_bios_table_string_entry_decode_handle(
    const struct pldm_bios_string_table_entry *entry);

/** @brief Get the string length for the entry
 *  @param[in] entry - Pointer to a bios string table entry
 *  @return Length of string in bytes
 */
uint16_t pldm_bios_table_string_entry_decode_string_length(
    const struct pldm_bios_string_table_entry *entry);

/** @brief Get the string from the entry
 *  @param[in] entry - Pointer to a bios string table entry
 *  @param[out] buffer - Pointer to a buffer to store the string
 *  @param[in] size - Size of the buffer to store the string
 *  @return Length of the string decoded
 */
uint16_t pldm_bios_table_string_entry_decode_string(
    const struct pldm_bios_string_table_entry *entry, char *buffer,
    size_t size);

/** @brief Get the string from the entry and check the validity of the
 * parameters
 *  @param[in] entry - Pointer to a bios string table entry
 *  @param[out] buffer - Pointer to a buffer to store the string
 *  @param[in] size - Size of the buffer to store the string
 *  @return pldm_completion_codes
 */
int pldm_bios_table_string_entry_decode_string_check(
    const struct pldm_bios_string_table_entry *entry, char *buffer,
    size_t size);

/** @brief Find an entry in bios string table by string
 *  @param[in] table - The BIOS String Table
 *  @param[in] length - Length of the BIOS String Table
 *  @param[in] str - String itself
 *  @return Pointer to an entry in the bios string table
 */
const struct pldm_bios_string_table_entry *
pldm_bios_table_string_find_by_string(const void *table, size_t length,
				      const char *str);
/** @brief Find an entry in bios string table by handle
 *  @param[in] table - The BIOS String Table
 *  @param[in] length - Length of the BIOS String Table
 *  @param[in] handle - Handle to identify a string in the bios string table
 *  @return Pointer to an entry in the bios string table
 */
const struct pldm_bios_string_table_entry *
pldm_bios_table_string_find_by_handle(const void *table, size_t length,
				      uint16_t handle);

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

/** @brief Get possible values string handles
 *  @param[in] entry - Pointer to bios attribute table entry
 *  @param[out] pv_hdls - Pointer to a buffer to stroe
 * PossibleValuesStringHandles
 *  @param[in] pv_num - Number of PossibleValuesStringHandles expected
 *  @return pldm_completion_codes
 */
uint8_t pldm_bios_table_attr_entry_enum_decode_pv_hdls(
    const struct pldm_bios_attr_table_entry *entry, uint16_t *pv_hdls,
    uint8_t pv_num);

/** @brief Get possible values string handles and check the validity of the
 * parameters
 *  @param[in] entry - Pointer to bios attribute table entry
 *  @param[out] pv_hdls - Pointer to a buffer to stroe
 * PossibleValuesStringHandles
 *  @param[in] pv_num - Number of PossibleValuesStringHandles the buffer can
 * stroe
 *  @return Number of PossibleValuesStringHandles decoded
 */
int pldm_bios_table_attr_entry_enum_decode_pv_hdls_check(
    const struct pldm_bios_attr_table_entry *entry, uint16_t *pv_hdls,
    uint8_t pv_num);

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
 */
void pldm_bios_table_attr_value_entry_encode_enum(
    void *entry, size_t entry_length, uint16_t attr_handle, uint8_t attr_type,
    uint8_t count, uint8_t *handle_indexes);

/** @brief Create an attribute value entry(type: enum) and check the validity of
 * the parameters
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
int pldm_bios_table_attr_value_entry_encode_enum_check(
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
 *  @param[out] entry - Pointer to bios attribute value entry
 *  @param[in] entry_length - Length of attribute value entry
 *  @param[in] attr_handle - This handle points to an attribute in the
 *  BIOS Attribute Vlaue Table.
 *  @param[in] attr_type - Type of this attribute in the BIOS Attribute Value
 * Table
 *  @param[in] string_length - Length of current string in bytes. 0 indicates
 * that the current string value is not set.
 *  @param[in] string - The current string itsel
 */
void pldm_bios_table_attr_value_entry_encode_string(
    void *entry, size_t entry_length, uint16_t attr_handle, uint8_t attr_type,
    uint16_t string_length, const char *string);
/** @brief Create an attribute value entry(type: string) and check the validity
 * of the parameters
 *  @param[out] entry - Pointer to bios attribute value entry
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
int pldm_bios_table_attr_value_entry_encode_string_check(
    void *entry, size_t entry_length, uint16_t attr_handle, uint8_t attr_type,
    uint16_t string_length, const char *string);

#ifdef __cplusplus
}
#endif

#endif
