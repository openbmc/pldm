#pragma once

#include "libpldm/platform.h"

#include "common/types.hpp"
#include "numeric_sensor.hpp"
#include "requester/handler.hpp"
#include "terminus.hpp"

#include <sdbusplus/server/object.hpp>
#include <sdeventplus/event.hpp>
#include <xyz/openbmc_project/Inventory/Item/Board/server.hpp>

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
    Terminus(pldm_tid_t tid, uint64_t supportedPLDMTypes);

    /** @brief Check if the terminus supports the PLDM type message
     *
     *  @param[in] type - PLDM Type
     *  @return support state - True if support, otherwise False
     */
    bool doesSupportType(uint8_t type);

    /** @brief Check if the terminus supports the PLDM command message
     *
     *  @param[in] type - PLDM Type
     *  @param[in] command - PLDM command
     *  @return support state - True if support, otherwise False
     */
    bool doesSupportCommand(uint8_t type, uint8_t command);

    /** @brief Set the supported PLDM commands for terminus
     *
     *  @param[in] cmds - bit mask of the supported PLDM commands
     *  @return success state - True if success, otherwise False
     */
    bool setSupportedCommands(std::vector<std::bitset<8>> cmds)
    {
        if (cmds.size() != PLDM_MAX_TYPES * (PLDM_MAX_CMDS_PER_TYPE / 8))
        {
            lg2::error(
                "setSupportedCommands invalid size {SIZE} of bit mask of the supported PLDM commands.",
                "SIZE", cmds.size());
            return false;
        }
        for (size_t i = 0; i < cmds.size(); i++)
        {
            supportedCmds[i] = cmds[i];
        }

        return true;
    };

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
    bool initialized = false;

    /** @brief maximum message buffer size the terminus can send and receive */
    uint16_t maxBufferSize;

    /** @brief This value indicates the event messaging styles supported by the
     * terminus */
    bitfield8_t synchronyConfigurationSupported;

    /** @brief The flag indicates that the terminus FIFO contains a large
     *         message that will require a multipart transfer via the
     *         PollForPlatformEvent command */
    bool pollEvent;

    /** @brief A list of numericSensors */
    std::vector<std::shared_ptr<NumericSensor>> numericSensors{};

    /** @brief Get Sensor Auxiliary Names by sensorID
     *
     *  @param[in] id - sensor ID
     *  @return sensor auxiliary names
     */
    std::shared_ptr<SensorAuxiliaryNames> getSensorAuxiliaryNames(SensorId id);

  private:
    /** @brief Contruct the NumericSensor sensor class for the PLDM sensor.
     *         The NumericSensor class will handle create D-Bus object path,
     *         provide the APIs to update sensor value, threshold...
     *
     *  @param[in] pdr - the numeric sensor PDR info
     */
    void addNumericSensor(
        const std::shared_ptr<pldm_numeric_sensor_value_pdr> pdr);

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

    /** @brief Contruct the NumericSensor sensor class for the compact numeric
     *         PLDM sensor.
     *
     *  @param[in] pdr - the compact numeric sensor PDR info
     */
    void addCompactNumericSensor(
        const std::shared_ptr<pldm_compact_numeric_sensor_pdr> pdr);

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

    /** @brief Store supported PLDM commands of a terminus
     *         Maximum number of PLDM Type is PLDM_MAX_TYPES
     *         Maximum number of PLDM command for each type is
     *         PLDM_MAX_CMDS_PER_TYPE.
     *         Each bitset<8> can store supported state of 8 PLDM commands.
     *         Size of supportedCmds will be
     *         PLDM_MAX_TYPES * (PLDM_MAX_CMDS_PER_TYPE / 8).
     */
    std::bitset<8> supportedCmds[PLDM_MAX_TYPES * (PLDM_MAX_CMDS_PER_TYPE / 8)];

    /* @brief Sensor Auxiliary Name list */
    std::vector<std::shared_ptr<SensorAuxiliaryNames>>
        sensorAuxiliaryNamesTbl{};

    /* @brief The pointer of iventory D-Bus interface for the terminus */
    std::unique_ptr<InventoryItemBoardIntf> inventoryItemBoardInft = nullptr;

    /* @brief Inventory D-Bus object path of the terminus */
    std::string inventoryPath;
};
} // namespace platform_mc
} // namespace pldm
