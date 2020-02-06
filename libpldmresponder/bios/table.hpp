#pragma once

#include <stdint.h>

#include <filesystem>
#include <vector>

#include "libpldm/bios.h"

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

class BIOSStringTable : public BIOSTable
{
  public:
    /** @brief Ctor - set file path to persist BIOS String table
     *
     *  @param[in] filePath - file where BIOS table should be persisted
     */
    BIOSStringTable(const char* filePath);

    /** @brief Find the string name from the BIOS string table for a string
     * handle
     *  @param[in] handle - string handle
     *  @return - std::string - name of the corresponding BIOS string
     */
    std::string findString(uint16_t handle) const;

  private:
    Table stringTable;
};

} // namespace bios
} // namespace responder
} // namespace pldm
