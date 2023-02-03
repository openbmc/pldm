#pragma once

#include "libpldm/platform.h"

#include "common/types.hpp"
#include "compact_numeric_sensor.hpp"
#include "numeric_effecter_dbus.hpp"
#include "numeric_sensor.hpp"

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
using InventoryItemBoardIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Board>;

/**
 * @brief Terminus
 *
 * Terminus class holds the TID, supported PLDM Type or PDRs which are needed by
 * other manager class for sensor monitoring and control.
 */
class Terminus
{
  public:
    Terminus(tid_t tid, uint64_t supportedPLDMTypes);

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
    tid_t getTid()
    {
        return tid;
    }

    /** @brief The setter to set terminus's mctp medium */
    void setMedium(std::string medium)
    {
        mctpMedium = medium;
        return;
    }

    /** @brief A list of PDRs fetched from Terminus */
    std::vector<std::vector<uint8_t>> pdrs{};

    /** @brief A flag to indicate if terminus has been initialzed */
    bool initalized = false;

    /** @brief A list of numericSensors */
    std::vector<std::shared_ptr<NumericSensor>> numericSensors{};

    /** @brief A list of compactNumericSensors */
    std::vector<std::shared_ptr<CompactNumericSensor>> compactNumericSensors{};

    /** @brief A list of numericEffecterDbus */
    std::vector<std::shared_ptr<NumericEffecterDbus>> numericEffecterDbus{};

    /** @brief A list of parsed numeric sensor PDRs */
    std::vector<std::shared_ptr<pldm_numeric_sensor_value_pdr>>
        numericSensorPdrs{};

    /** @brief A list of parsed compact numeric sensor PDRs */
    std::vector<std::shared_ptr<pldm_compact_numeric_sensor_pdr>>
        compactNumericSensorPdrs{};

    /** @brief A list of parsed numeric effecter PDRs */
    std::vector<std::shared_ptr<pldm_numeric_effecter_value_pdr>>
        numericEffecterPdrs{};

    /** @brief Get Sensor Auxiliary Names by sensorID
     *
     *  @param[in] id - sensor ID
     *  @return sensor auxiliary names
     */
    std::shared_ptr<SensorAuxiliaryNames> getSensorAuxiliaryNames(SensorId id);

    /** @brief Add a numeric sensor to terminus
     *
     *  @param[in] pdr - the numeric sensor pdr
     */
    void addNumericSensor(
        const std::shared_ptr<pldm_numeric_sensor_value_pdr> pdr);

    void addCompactNumericSensor(
        const std::shared_ptr<pldm_compact_numeric_sensor_pdr> pdr);

    void addNumericEffecterDbus(
        const std::shared_ptr<pldm_numeric_effecter_value_pdr> pdr);

  private:
    std::shared_ptr<pldm_numeric_sensor_value_pdr>
        parseNumericSensorPDR(const std::vector<uint8_t>& pdrData);

    std::shared_ptr<pldm_compact_numeric_sensor_pdr>
        parseCompactNumericSensorPDR(const std::vector<uint8_t>& pdrData);

    std::shared_ptr<pldm_numeric_effecter_value_pdr>
        parseNumericEffecterPDR(const std::vector<uint8_t>& pdrData);

    std::shared_ptr<SensorAuxiliaryNames>
        parseSensorAuxiliaryNamesPDR(const std::vector<uint8_t>& pdrData);

    tid_t tid;
    std::bitset<64> supportedTypes;

    std::vector<std::shared_ptr<SensorAuxiliaryNames>>
        sensorAuxiliaryNamesTbl{};

    std::unique_ptr<InventoryItemBoardIntf> inventoryItemBoardInft = nullptr;
    std::string inventoryPath;
    MctpMedium mctpMedium;

    std::shared_ptr<SensorAuxiliaryNames>
        parseCompactNumericSensorNames(const std::vector<uint8_t>& pdrData);
    mctp_eid_t eid;
};
} // namespace platform_mc
} // namespace pldm
