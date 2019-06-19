#pragma once

#include <stdint.h>

#include <filesystem>
#include <fstream>
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
 *  @brief Provides APIs for storing, loading and caching BIOS tables
 *
 *  Typical usage is as follows:
 *  static BIOSTable table(BIOS_STRING_TABLE_FILE_PATH);
 *  if (table.isEmpty()) { // no persisted table, no cache
 *    // construct BIOSTable 't'
 *    // prepare Response 'r'
 *    // send response to GetBIOSTable
 *    table.store(t); // persisted
 *    table.cacheResponse(std::move(r)); // cached
 *  }
 *  else if (!table.hasResponse()) { // no cache, but table persists (eg reboot)
 *    // create Response 'r' which has response fields (except
 *    // the table itself) to a GetBIOSTable command
 *    table.load(r); // actual table will be pushed back to the vector
 *    // send response to GetBIOSTable
 *    table.cacheResponse(std::move(r)); // cached
 *  }
 *  else { // cache present
 *    Response r = table.getResponse();
 *    // send response to GetBIOSTable
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
    bool isEmpty() const;

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

    /** @brief Cache a PLDM response message to GetBIOSTable
     *
     *  @param[in] response - PLDM response to GetBIOSTable
     */
    void cacheGetTableResponse(Response&& r)
    {
        response = std::move(r);
    }

    /** @brief Retrieve cached PLDM response to GetBIOSTable
     *
     *  @return Response - cached PLDM response message
     */
    Response getTableResponse() const
    {
        return response;
    }

    /** @brief Checks if there's a cached PLDM response to GetBIOSTable
     *
     *  @return bool - true if cached rsponse exists, false otherwise
     */
    bool hasResponse() const
    {
        return !response.empty();
    }

  private:
    // file storing PLDM BIOS table
    fs::path filePath;
    // Cached PLDM response to GetBIOSTable
    Response response{};
};

} // namespace bios
} // namespace responder
} // namespace pldm
