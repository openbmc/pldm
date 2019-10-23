#pragma once

#include "config.h"

#include "fru_parser.hpp"

#include <map>
#include <sdbusplus/message.hpp>
#include <string>
#include <variant>
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

using DbusInterface = std::string;
using DbusProperty = std::string;
using Value =
    std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t,
                 uint64_t, double, std::string, std::vector<uint8_t>>;
using PropertyMap = std::map<DbusProperty, Value>;
using DbusInterfaceMap = std::map<DbusInterface, PropertyMap>;
using ObjectValueTree =
    std::map<sdbusplus::message::object_path, DbusInterfaceMap>;

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
    FruIntf(T& t) : impl(t)
    {
    }

    /** @brief Total length of the FRU table in bytes, this excludes the pad
     *         bytes and the checksum.
     *
     *  @return size of the FRU table
     */
    uint32_t size() const
    {
        return impl.size();
    }

    uint32_t checkSum() const
    {
        return impl.checkSum();
    }

    uint16_t numRSI() const
    {
        return impl.numRSI();
    }

    uint16_t numRecords() const
    {
        return impl.numRecords();
    }

    void getFRUTable(Response& response)
    {
        return impl.getFRUTable(response);
    }

  private:
    T& impl;
};

class FruImpl
{
  public:
    /* @brief Header size for FRU record, it includes the FRU record set
     *        identifier, FRU record type, Number of FRU fields, Encoding type
     *        of FRU fields
     */
    static constexpr size_t recHeaderSize =
        sizeof(struct pldm_fru_record_data_format) -
        sizeof(struct pldm_fru_record_tlv);

    /** @brief The file table is initialised by parsing the config file
     *         containing information about the files.
     *
     * @param[in] fileTableConfigPath - path to the json file containing
     *                                  information
     */
    FruImpl(const std::string& configPath);

    /** @brief Total length of the FRU table in bytes, this excludes the pad
     *         bytes and the checksum.
     *
     *  @return size of the FRU table
     */
    uint32_t size() const
    {
        return table.size() - padBytes;
    }

    uint32_t checkSum() const
    {
        return checksum;
    }

    uint16_t numRSI() const
    {
        return rsi;
    }

    uint16_t numRecords() const
    {
        return numrecs;
    }

    void getFRUTable(Response& response);

  private:
    uint16_t nextRSI()
    {
        return ++rsi;
    }

    uint16_t rsi = 0;
    uint16_t numrecs = 0;
    uint8_t padBytes = 0;
    std::vector<uint8_t> table;
    uint32_t checksum = 0;

    void populateFRUTable();

    /** @brief Parse the FRU_Master.json file and populate the D-Bus lookup
     *         information which provides the service, root D-Bus path and the
     *         item interfaces.
     *
     *  @param[in] filePath - file path to FRU_Master.json
     */
    void populateRecord(const pldm::responder::DbusInterfaceMap& interfaces,
                        const fru_parser::FruRecordInfos& recordInfos);
};

} // namespace pldm
