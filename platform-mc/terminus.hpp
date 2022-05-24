#pragma once

#include "libpldm/platform.h"

#include "common/types.hpp"
#include "requester/handler.hpp"

#include <sdbusplus/server/object.hpp>
#include <sdeventplus/event.hpp>
#include <xyz/openbmc_project/Inventory/Item/Board/server.hpp>

using namespace pldm::pdr;

namespace pldm
{
namespace platform_mc
{
using SensorId = uint16_t;
using SensorCnt = uint8_t;
using NameLanguageTag = std::string;
using SensorName = std::string;
using SensorAuxiliaryNames = std::tuple<
    SensorId, SensorCnt,
    std::vector<std::vector<std::pair<NameLanguageTag, SensorName>>>>;

/**
 * @brief Terminus
 *
 * Terminus class holds the TID, supported PLDM Type or PDRs which are needed by
 * other manager class for sensor monitoring and control.
 */
class Terminus
{
  public:
    Terminus(pldm_tid_t tid, uint64_t supportedPLDMTypes);

    /** @brief Check if the terminus supports the PLDM type message
     *
     *  @param[in] type - PLDM Type
     */
    bool doesSupport(uint8_t type);

    /** @brief Parse the PDRs stored in the member variable, pdrs.
     *
     *  @return False if any unsupported PDR is detected.
     */
    bool parsePDRs();

    /** @brief The getter to return terminus's TID */
    pldm_tid_t getTid()
    {
        return tid;
    }

    /** @brief A list of PDRs fetched from Terminus */
    std::vector<std::vector<uint8_t>> pdrs{};

    /** @brief A flag to indicate if terminus has been initialzed */
    bool initalized = false;

    /** @brief A flag to indicate if terminus class is constructed without
     *         error.
     */
    bool isConstructed = false;

    /** @brief Get Sensor Auxiliary Names by sensorID
     *
     *  @param[in] id - sensor ID
     *  @return sensor auxiliary names
     */
    std::shared_ptr<SensorAuxiliaryNames> getSensorAuxiliaryNames(SensorId id);

  private:
    /** @brief Parse the numeric sensor PDRs
     *
     *  @param[in] pdrData - the response PDRs from GetPDR command
     *  @return pointer to numeric sensor info struct
     */
    std::shared_ptr<pldm_numeric_sensor_value_pdr>
        parseNumericSensorPDR(const std::vector<uint8_t>& pdrData);

    /** @brief Parse the sensor Auxiliary name PDRs
     *
     *  @param[in] pdrData - the response PDRs from GetPDR command
     *  @return pointer to sensor Auxiliary name info struct
     */
    std::shared_ptr<SensorAuxiliaryNames>
        parseSensorAuxiliaryNamesPDR(const std::vector<uint8_t>& pdrData);

    /** @brief Parse the compact numeric sensor PDRs
     *
     *  @param[in] pdrData - the response PDRs from GetPDR command
     *  @return pointer to compact numeric sensor info struct
     */
    std::shared_ptr<pldm_compact_numeric_sensor_pdr>
        parseCompactNumericSensorPDR(const std::vector<uint8_t>& pdrData);

    /** @brief Parse the sensor Auxiliary name from compact numeric sensor PDRs
     *
     *  @param[in] pdrData - the response PDRs from GetPDR command
     *  @return pointer to sensor Auxiliary name info struct
     */
    std::shared_ptr<SensorAuxiliaryNames>
        parseCompactNumericSensorNames(const std::vector<uint8_t>& pdrData);

    /* @brief The terminus's TID */
    pldm_tid_t tid;

    /* @brief The supported PLDM command types of the terminus */
    std::bitset<64> supportedTypes;

    /* @brief Sensor Auxiliary Name list */
    std::vector<std::shared_ptr<SensorAuxiliaryNames>>
        sensorAuxiliaryNamesTbl{};
};
} // namespace platform_mc
} // namespace pldm
