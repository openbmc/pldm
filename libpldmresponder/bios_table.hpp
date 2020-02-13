#pragma once

#include <stdint.h>

#include <filesystem>
#include <string>
#include <vector>

#include "libpldm/bios.h"
#include "libpldm/bios_table.h"

namespace pldm
{

namespace responder
{

namespace bios
{

using Table = std::vector<uint8_t>;
using Response = std::vector<uint8_t>;
namespace fs = std::filesystem;

/** @class BIOSTable
 *
 *  @brief Provides APIs for storing and loading BIOS tables
 *
 *  Typical usage is as follows:
 *  BIOSTable table(BIOS_STRING_TABLE_FILE_PATH);
 *  if (table.isEmpty()) { // no persisted table
 *    // construct BIOSTable 't'
 *    // prepare Response 'r'
 *    // send response to GetBIOSTable
 *    table.store(t); // persisted
 *  }
 *  else { // persisted table exists
 *    // create Response 'r' which has response fields (except
 *    // the table itself) to a GetBIOSTable command
 *    table.load(r); // actual table will be pushed back to the vector
 *  }
 */
class BIOSTable
{
  public:
    /** @brief Ctor - set file path to persist BIOS table
     *
     *  @param[in] filePath - file where BIOS table should be persisted
     */
    BIOSTable(const char* filePath);

    /** @brief Checks if there's a persisted BIOS table
     *
     *  @return bool - true if table exists, false otherwise
     */
    bool isEmpty() const noexcept;

    /** @brief Persist a BIOS table(string/attribute/attribute value)
     *
     *  @param[in] table - BIOS table
     */
    void store(const Table& table);

    /** @brief Load BIOS table from persistent store to memory
     *
     *  @param[in,out] response - PLDM response message to GetBIOSTable
     *  (excluding table), table will be pushed back to this.
     */
    void load(Response& response) const;

  private:
    // file storing PLDM BIOS table
    fs::path filePath;
};

/** @class BIOSStringTableInterface
 *  @brief Provide interfaces to the BIOS string table operations
 */
class BIOSStringTableInterface
{
  public:
    virtual ~BIOSStringTableInterface() = default;

    virtual std::string findString(uint16_t handle) const = 0;

    virtual uint16_t findHandle(const std::string& name) const = 0;
};

/** @class BIOSStringTable
 *  @brief Collection of BIOS string table operations.
 */
class BIOSStringTable : public BIOSStringTableInterface
{
  public:
    /** @brief Constructs BIOSStringTable
     *
     *  @param[in] stringTable - The stringTable in RAM
     */
    BIOSStringTable(const Table& stringTable);

    /** @brief Constructs BIOSStringTable
     *
     *  @param[in] biosTable - The BIOSTable
     */
    BIOSStringTable(const BIOSTable& biosTable);

    /** @brief Find the string name from the BIOS string table for a string
     * handle
     *  @param[in] handle - string handle
     *  @return name of the corresponding BIOS string
     *  @throw std::invalid_argument if the string can not be found.
     */
    std::string findString(uint16_t handle) const override;

    /** @brief Find the string handle from the BIOS string table by the given
     *         name
     *  @param[in] name - name of the BIOS string
     *  @return handle of the string
     *  @throw std::invalid_argument if the string can not be found
     */
    uint16_t findHandle(const std::string& name) const override;

    /** @brief Get the string handle for the entry
     *  @param[in] entry - Pointer to a bios string table entry
     *  @return Handle to identify a string in the bios string table
     */
    static uint16_t decodeHandle(const pldm_bios_string_table_entry* entry);

    /** @brief Get the string from the entry
     *  @param[in] entry - Pointer to a bios string table entry
     *  @return The String
     */
    static std::string decodeString(const pldm_bios_string_table_entry* entry);

  private:
    Table stringTable;
};

class BIOSAttrTable
{
  public:
    struct TableHeader
    {
        uint16_t attrHandle;
        uint8_t attrType;
        uint16_t stringHandle;
    };

    static TableHeader decodeHeader(const pldm_bios_attr_table_entry* entry);

    struct StringField
    {
        uint8_t stringType;
        uint16_t minLength;
        uint16_t maxLength;
        uint16_t defLength;
        std::string defString;
    };

    static StringField
        decodeStringEntry(const pldm_bios_attr_table_entry* entry);

    static const pldm_bios_attr_table_entry*
        constructStringEntry(Table& table,
                             pldm_bios_table_attr_entry_string_info* info);
    static const pldm_bios_attr_table_entry*
        constructIntegerEntry(Table& table,
                              pldm_bios_table_attr_entry_integer_info* info);
};

class BIOSAttrValTable
{
  public:
    struct TableHeader
    {
        uint16_t attrHandle;
        uint8_t attrType;
    };

    static TableHeader
        decodeHeader(const pldm_bios_attr_val_table_entry* entry);

    static std::string
        decodeStringEntry(const pldm_bios_attr_val_table_entry* entry,
                          uint8_t strType);
    static uint64_t
        decodeIntegerEntry(const pldm_bios_attr_val_table_entry* entry);

    static const pldm_bios_attr_val_table_entry*
        constructStringEntry(Table& table, uint16_t attrHandle,
                             uint8_t attrType, const std::string& str);
    static const pldm_bios_attr_val_table_entry*
        constructIntegerEntry(Table& table, uint16_t attrHandle,
                              uint8_t attrType, uint64_t value);
};

} // namespace bios
} // namespace responder
} // namespace pldm
