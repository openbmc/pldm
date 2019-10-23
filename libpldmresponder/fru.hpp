#pragma once

#include "config.h"

#include <vector>

#include "libpldm/fru.h"

namespace pldm
{

using Response = std::vector<uint8_t>;

namespace responder
{

namespace fru
{

/** @brief Register handlers for commands from the FRU spec
 */
void registerHandlers();

} // namespace fru

/** @brief Handler for GetFRURecordTableMetadata
 *
 *  @param[in] request - Request message payload
 *  @param[in] payloadLength - Request payload length
 *  @param[out] Response - Response message written here
 */
Response getFRURecordTableMetadata(const pldm_msg* request,
                                   size_t payloadLength);

Response getFRURecordTable(const pldm_msg* request, size_t payloadLength);

} // namespace responder

template <typename T>
struct FruIntf
{
  public:
    FruIntf(const T& t) : impl(t)
    {
    }

    uint32_t size() const
    {
        return impl.size();
    }

    uint32_t checksum() const
    {
        return impl.checksum();
    }

    uint16_t numRSI() const
    {
        return impl.numRSI();
    }

    uint16_t numRecords() const
    {
        return impl.numRecords();
    }

    void populate(Response& response) const
    {
        impl.populate(response);
    }

  private:
    const T& impl;
};

struct FruImpl
{
  public:
    uint32_t size() const;

    uint32_t checksum() const;

    uint16_t numRSI() const;

    uint16_t numRecords() const;

    void populate(Response& response) const;
};

} // namespace pldm
