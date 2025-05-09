#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"

#include <libpldm/pdr.h>

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>

PHOSPHOR_LOG2_USING;

using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

namespace fs = std::filesystem;

namespace pldm
{
namespace responder
{
namespace pdr_utils
{
/** @struct Type ID associated with pdr
 *
 */
enum class TypeId
{
    PLDM_EFFECTER_ID,
    PLDM_SENSOR_ID
};

struct FruTLV
{
    uint8_t fruFieldType;
    uint8_t fruFieldLen;
    std::vector<uint8_t> fruFieldValue;
};

struct FruRecordDataFormat
{
    uint16_t fruRSI;
    uint8_t fruRecType;
    uint8_t fruNum;
    uint8_t fruEncodeType;
    std::vector<FruTLV> fruTLV;
};

/** @struct PdrEntry
 *  PDR entry structure that acts as a PDR record structure in the PDR
 *  repository to handle PDR APIs.
 */
struct PdrEntry
{
    uint8_t* data;
    uint32_t size;
    union
    {
        uint32_t recordHandle;
        uint32_t nextRecordHandle;
    } handle;
};
using Type = uint8_t;
using Json = nlohmann::json;
using RecordHandle = uint32_t;
using State = uint8_t;
using PossibleValues = std::vector<uint8_t>;

/** @brief Map of DBus property State to attribute value
 */
using StatestoDbusVal = std::map<State, pldm::utils::PropertyValue>;
using DbusMappings = std::vector<pldm::utils::DBusMapping>;
using DbusValMaps = std::vector<StatestoDbusVal>;
using EventStates = std::array<uint8_t, 8>;

/** @brief Parse PDR JSON file and output Json object
 *
 *  @param[in] path - path of PDR JSON file
 *
 *  @return Json - Json object
 */
inline Json readJson(const std::string& path)
{
    fs::path dir(path);
    if (!fs::exists(dir) || fs::is_empty(dir))
    {
        throw InternalFailure();
    }

    std::ifstream jsonFile(path);
    if (!jsonFile.is_open())
    {
        error("Error opening PDR JSON file at '{PATH}'", "PATH", path);
        return {};
    }

    return Json::parse(jsonFile);
}

/** @brief Populate the mapping between D-Bus property stateId and attribute
 *          value for the effecter PDR enumeration attribute.
 *
 *  @param[in] type - type of the D-Bus property
 *  @param[in] dBusValues - json array of D-Bus property values
 *  @param[in] pv - Possible values for the effecter PDR enumeration attribute
 *
 *  @return StatestoDbusVal - Map of D-Bus property stateId to attribute value
 */
StatestoDbusVal populateMapping(const std::string& type, const Json& dBusValues,
                                const PossibleValues& pv);

/**
 *  @class RepoInterface
 *
 *  @brief Abstract class describing the interface API to the PDR repository,
 *         this class wraps operations used to handle the new PDR APIs.
 */
class RepoInterface
{
  public:
    RepoInterface(pldm_pdr* repo) : repo(repo) {}

    virtual ~RepoInterface() = default;

    /** @brief Get an opaque pldm_pdr structure
     *
     *  @return pldm_pdr - pldm_pdr structure
     */
    virtual pldm_pdr* getPdr() const = 0;

    /** @brief Add a PDR record to a PDR repository
     *
     *  @param[in] pdrEntry - PDR records entry(data, size, recordHandle)
     *
     *  @return uint32_t - record handle assigned to PDR record
     */
    virtual RecordHandle addRecord(const PdrEntry& pdrEntry) = 0;

    /** @brief Get the first PDR record from a PDR repository
     *
     *  @param[in] pdrEntry - PDR records entry(data, size, nextRecordHandle)
     *
     *  @return opaque pointer acting as PDR record handle, will be NULL if
     *          record was not found
     */
    virtual const pldm_pdr_record* getFirstRecord(PdrEntry& pdrEntry) = 0;

    /** @brief Get the next PDR record from a PDR repository
     *
     *  @param[in] currRecord - opaque pointer acting as a PDR record handle
     *  @param[in] pdrEntry - PDR records entry(data, size, nextRecordHandle)
     *
     *  @return opaque pointer acting as PDR record handle, will be NULL if
     *          record was not found
     */
    virtual const pldm_pdr_record* getNextRecord(
        const pldm_pdr_record* currRecord, PdrEntry& pdrEntry) = 0;

    /** @brief Get record handle of a PDR record
     *
     *  @param[in] record - opaque pointer acting as a PDR record handle
     *
     *  @return uint32_t - record handle assigned to PDR record; 0 if record is
     *                     not found
     */
    virtual uint32_t getRecordHandle(const pldm_pdr_record* record) const = 0;

    /** @brief Get number of records in a PDR repository
     *
     *  @return uint32_t - number of records
     */
    virtual uint32_t getRecordCount() = 0;

    /** @brief Determine if records are empty in a PDR repository
     *
     *  @return bool - true means empty and false means not empty
     */
    virtual bool empty() = 0;

  protected:
    pldm_pdr* repo;
};

/**
 *  @class Repo
 *
 *  Wrapper class to handle the PDR APIs
 *
 *  This class wraps operations used to handle PDR APIs.
 */
class Repo : public RepoInterface
{
  public:
    Repo(pldm_pdr* repo) : RepoInterface(repo) {}

    pldm_pdr* getPdr() const override;

    RecordHandle addRecord(const PdrEntry& pdrEntry) override;

    const pldm_pdr_record* getFirstRecord(PdrEntry& pdrEntry) override;

    const pldm_pdr_record* getNextRecord(const pldm_pdr_record* currRecord,
                                         PdrEntry& pdrEntry) override;

    uint32_t getRecordHandle(const pldm_pdr_record* record) const override;

    uint32_t getRecordCount() override;

    bool empty() override;
};

/** @brief Parse the State Sensor PDR and return the parsed sensor info which
 *         will be used to lookup the sensor info in the PlatformEventMessage
 *         command of sensorEvent type.
 *
 *  @param[in] stateSensorPdr - state sensor PDR
 *
 *  @return terminus handle, sensor ID and parsed sensor info
 */
std::tuple<pldm::pdr::TerminusHandle, pldm::pdr::SensorID,
           pldm::pdr::SensorInfo>
    parseStateSensorPDR(const std::vector<uint8_t>& stateSensorPdr);

/** @brief Parse FRU record table and return the vector of the FRU record data
 *         format structure
 *
 *  @param[in] fruData - fru data
 *  @param[in] fruLen  - fru len
 *
 *  @return std::vector<FruRecordDataFormat> - the vector of the FRU record data
 *          format structure
 */
std::vector<FruRecordDataFormat> parseFruRecordTable(const uint8_t* fruData,
                                                     size_t fruLen);

/** @brief Return the size of data type based on the effecterDataSize enum value
 *
 *  @param[in] effecterDataSize - Bitwidth and format of setting effecter value
 *  @return[out] Map the effecterDataSize enum value to datatype and return the
 *               size of dataType
 */
size_t getEffecterDataSize(uint8_t effecterDataSize);

} // namespace pdr_utils
} // namespace responder
} // namespace pldm
