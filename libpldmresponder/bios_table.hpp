#pragma once

#include <stdint.h>

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

/** @class Table
 *
 *  @brief Provides APIs for storing, loading and caching BIOS tables
 *
 *  Typical usage is as follows:
 *  static Table table(BIOS_STRING_TABLE_FILE_PATH);
 *  if (table.isEmpty()) { // no persisted table, no cache
 *    // construct Table 't'
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
class Repo
{
    public:
        /** @brief Ctor - set file path to persist BIOS table
         *
         *  @param[in] filePath - file where BIOS table should be persisted
         */
        Table(const char* filePath);

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
        void cacheResponse(Response&& response);

        /** @brief Retrieve cached PLDM response to GetBIOSTable
         *
         *  @return Response - cached PLDM response message
         */
        Response getResponse() const;

        /** @brief Checks if there's a cached PLDM response to GetBIOSTable
         *
         *  @return bool - true if cached rsponse exists, false otherwise
         */
        bool hasResponse() const;

    private:
        // file storing PLDM BIOS table
        fs::path filePath;
        // Cached PLDM response to GetBIOSTable
        Response response;
};

} // namespace bios
} // namespace responder
} // namespace pldm
