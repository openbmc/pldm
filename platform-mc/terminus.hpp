#pragma once

#include "common/types.hpp"
#include "dbus_impl_fru.hpp"
#include "numeric_sensor.hpp"
#include "requester/handler.hpp"
#include "terminus.hpp"

#include <libpldm/fru.h>
#include <libpldm/platform.h>

#include <sdbusplus/server/object.hpp>
#include <sdeventplus/event.hpp>

#include <algorithm>
#include <bitset>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace pldm
{
namespace platform_mc
{

using ContainerID = uint16_t;
using EntityInstanceNumber = uint16_t;
using EntityName = std::string;
using EntityType = uint16_t;
using SensorId = uint16_t;
using SensorCnt = uint8_t;
using NameLanguageTag = std::string;
using SensorName = std::string;
using SensorAuxiliaryNames = std::tuple<
    SensorId, SensorCnt,
    std::vector<std::vector<std::pair<NameLanguageTag, SensorName>>>>;

/** @struct EntityKey
 *
 *  EntityKey uniquely identifies the PLDM entity and a combination of Entity
 *  Type, Entity Instance Number, Entity Container ID
 *
 */
struct EntityKey
{
    EntityType type;                  //!< Entity type
    EntityInstanceNumber instanceIdx; //!< Entity instance number
    ContainerID containerId;          //!< Entity container ID

    bool operator==(const EntityKey& e) const
    {
        return ((type == e.type) && (instanceIdx == e.instanceIdx) &&
                (containerId == e.containerId));
    }
};

using AuxiliaryNames = std::vector<std::pair<NameLanguageTag, std::string>>;
using EntityKey = struct EntityKey;
using EntityAuxiliaryNames = std::tuple<EntityKey, AuxiliaryNames>;

/**
 * @brief Terminus
 *
 * Terminus class holds the TID, supported PLDM Type or PDRs which are needed by
 * other manager class for sensor monitoring and control.
 */
class Terminus
{
  public:
    Terminus(pldm_tid_t tid, uint64_t supportedPLDMTypes,
             sdeventplus::Event& event);

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
    bool setSupportedCommands(const std::vector<uint8_t>& cmds)
    {
        const size_t expectedSize =
            PLDM_MAX_TYPES * (PLDM_MAX_CMDS_PER_TYPE / 8);
        if (cmds.empty() || cmds.size() != expectedSize)
        {
            lg2::error(
                "setSupportedCommands received invalid bit mask size. Expected: {EXPECTED}, Received: {RECEIVED}",
                "EXPECTED", expectedSize, "RECEIVED", cmds.size());
            return false;
        }

        /* Assign Vector supportedCmds by Vector cmds */
        supportedCmds.resize(cmds.size());
        std::copy(cmds.begin(), cmds.begin() + cmds.size(),
                  supportedCmds.begin());

        return true;
    }

    /** @brief Set the PLDM supported type version for terminus
     *
     *  @param[in] type - PLDM supported types
     *  @param[in] version - PLDM supported type version
     *  @return success state - True if success, otherwise False
     */
    inline bool setSupportedTypeVersions(const uint8_t type,
                                         const ver32_t version)
    {
        if (type > PLDM_MAX_TYPES || type >= supportedTypeVersions.size())
        {
            return false;
        }
        supportedTypeVersions[type] = version;

        return true;
    }

    /** @brief Parse the PDRs stored in the member variable, pdrs.
     */
    void parseTerminusPDRs();

    /** @brief The getter to return terminus's TID */
    pldm_tid_t getTid()
    {
        return tid;
    }

    /** @brief The getter to get terminus's mctp medium */
    std::optional<std::string_view> getTerminusName()
    {
        if (terminusName.empty())
        {
            return std::nullopt;
        }
        return terminusName;
    }

    /** @brief Parse record data from FRU table
     *
     *  @param[in] fruData - pointer to FRU record table
     *  @param[in] fruLen - FRU table length
     */
    void updateInventoryWithFru(const uint8_t* fruData, const size_t fruLen);

    /** @brief A list of PDRs fetched from Terminus */
    std::vector<std::vector<uint8_t>> pdrs{};

    /** @brief A flag to indicate if terminus has been initialized */
    bool initialized = false;

    /** @brief maximum message buffer size the terminus can send and receive */
    uint16_t maxBufferSize;

    /** @brief This value indicates the event messaging styles supported by the
     *         terminus
     */
    bitfield8_t synchronyConfigurationSupported;

    /** @brief A list of numericSensors */
    std::vector<std::shared_ptr<NumericSensor>> numericSensors{};

    /** @brief The flag indicates that the terminus FIFO contains a large
     *         message that will require a multipart transfer via the
     *         PollForPlatformEvent command
     */
    bool pollEvent;

    /** @brief The sensor id is used in pollForPlatformMessage command */
    uint16_t pollEventId;

    /** @brief The dataTransferHandle from `pldmMessagePollEvent` event and will
     *         be used as `dataTransferHandle` for pollForPlatformMessage
     *         command.
     */
    uint32_t pollDataTransferHandle;

    /** @brief Get Sensor Auxiliary Names by sensorID
     *
     *  @param[in] id - sensor ID
     *  @return sensor auxiliary names
     */
    std::shared_ptr<SensorAuxiliaryNames> getSensorAuxiliaryNames(SensorId id);

    /** @brief Get Numeric Sensor Object by sensorID
     *
     *  @param[in] id - sensor ID
     *
     *  @return sensor object
     */
    std::shared_ptr<NumericSensor> getSensorObject(SensorId id);

  private:
    /** @brief Find the Terminus Name from the Entity Auxiliary name list
     *         The Entity Auxiliary name list is entityAuxiliaryNamesTbl.
     *  @return terminus name in string option
     */
    std::optional<std::string_view> findTerminusName();

    /** @brief Construct the NumericSensor sensor class for the PLDM sensor.
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
    std::shared_ptr<pldm_numeric_sensor_value_pdr> parseNumericSensorPDR(
        const std::vector<uint8_t>& pdrData);

    /** @brief Parse the sensor Auxiliary name PDRs
     *
     *  @param[in] pdrData - the response PDRs from GetPDR command
     *  @return pointer to sensor Auxiliary name info struct
     */
    std::shared_ptr<SensorAuxiliaryNames> parseSensorAuxiliaryNamesPDR(
        const std::vector<uint8_t>& pdrData);

    /** @brief Parse the Entity Auxiliary name PDRs
     *
     *  @param[in] pdrData - the response PDRs from GetPDR command
     *  @return pointer to Entity Auxiliary name info struct
     */
    std::shared_ptr<EntityAuxiliaryNames> parseEntityAuxiliaryNamesPDR(
        const std::vector<uint8_t>& pdrData);

    /** @brief Construct the NumericSensor sensor class for the compact numeric
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
    std::shared_ptr<SensorAuxiliaryNames> parseCompactNumericSensorNames(
        const std::vector<uint8_t>& pdrData);

    /** @brief Create the terminus inventory path to
     *         /xyz/openbmc_project/inventory/Item/Board/.
     *
     *  @param[in] tName - the terminus name
     *  @return true/false: True if there is no error in creating inventory path
     *
     */
    bool createInventoryPath(std::string tName);

    /** @brief Get sensor names from Sensor Auxiliary Names PDRs
     *
     *  @param[in] sensorId - Sensor ID
     *  @param[in] isEffecter - This is an effecter, not a sensor
     *  @return vector of sensor name strings
     *
     */
    std::vector<std::string> getSensorNames(const SensorId& sensorId);

    /** @brief Add the next sensor PDR to this terminus, iterated by
     *         sensorPdrIt.
     */
    void addNextSensorFromPDRs();

    /* @brief The terminus's TID */
    pldm_tid_t tid;

    /* @brief The supported PLDM command types of the terminus */
    std::bitset<64> supportedTypes;

    /** @brief Store supported PLDM commands of a terminus
     *         Maximum number of PLDM Type is PLDM_MAX_TYPES
     *         Maximum number of PLDM command for each type is
     *         PLDM_MAX_CMDS_PER_TYPE.
     *         Each uint8_t can store the supported state of 8 PLDM commands.
     *         Size of supportedCmds will be
     *         PLDM_MAX_TYPES * (PLDM_MAX_CMDS_PER_TYPE / 8).
     */
    std::vector<uint8_t> supportedCmds;

    /* @brief The PLDM supported type version */
    std::map<uint8_t, ver32_t> supportedTypeVersions;

    /* @brief Sensor Auxiliary Name list */
    std::vector<std::shared_ptr<SensorAuxiliaryNames>>
        sensorAuxiliaryNamesTbl{};

    /* @brief Entity Auxiliary Name list */
    std::vector<std::shared_ptr<EntityAuxiliaryNames>>
        entityAuxiliaryNamesTbl{};

    /** @brief Terminus name */
    EntityName terminusName{};
    /* @brief The pointer of inventory D-Bus interface for the terminus */
    std::unique_ptr<pldm::dbus_api::PldmEntityReq> inventoryItemBoardInft =
        nullptr;

    /* @brief Inventory D-Bus object path of the terminus */
    std::string inventoryPath;

    /** @brief reference of main event loop of pldmd, primarily used to schedule
     *  work
     */
    sdeventplus::Event& event;

    /** @brief The event source to defer sensor creation tasks to event loop*/
    std::unique_ptr<sdeventplus::source::Defer> sensorCreationEvent;

    /** @brief Numeric Sensor PDR list */
    std::vector<std::shared_ptr<pldm_numeric_sensor_value_pdr>>
        numericSensorPdrs{};

    /** @brief Compact Numeric Sensor PDR list */
    std::vector<std::shared_ptr<pldm_compact_numeric_sensor_pdr>>
        compactNumericSensorPdrs{};

    /** @brief Iteration to loop through sensor PDRs when adding sensors */
    SensorId sensorPdrIt = 0;
};
} // namespace platform_mc
} // namespace pldm
