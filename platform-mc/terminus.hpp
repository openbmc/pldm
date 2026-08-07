#pragma once

#include "common/types.hpp"
#include "dbus_impl_fru.hpp"
#include "entity.hpp"
#include "numeric_sensor.hpp"
#include "state_sensor.hpp"

#include <libpldm/fru.h>
#include <libpldm/pdr.h>
#include <libpldm/platform.h>

#include <sdbusplus/server/object.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/source/event.hpp>

#include <algorithm>
#include <bitset>
#include <string>
#include <vector>

namespace pldm
{
namespace platform_mc
{

using namespace pldm::pdr;

/** @struct EntityAssociation
 *
 *  The containment record of one Entity Association PDR: the container entity
 *  and the entities it contains.
 */
struct EntityAssociation
{
    EntityKey container;             //!< Container entity
    std::vector<EntityKey> children; //!< Contained entities
};

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
        if (type > PLDM_MAX_TYPES)
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
    pldm_tid_t getTid() const
    {
        return tid;
    }

    /** @brief The setter to set terminus's mctp medium */
    void setTerminusName(const EntityName& tName)
    {
        terminusName = tName;
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

    /** @brief A list of stateSensors */
    std::vector<std::shared_ptr<StateSensor>> stateSensors{};

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
    std::shared_ptr<SensorAuxiliaryNames> getSensorAuxiliaryNames(SensorID id);

    /** @brief Get Numeric Sensor Object by sensorID
     *
     *  @param[in] id - sensor ID
     *
     *  @return sensor object
     */
    std::shared_ptr<NumericSensor> getSensorObject(SensorID id);

    /** @brief Get the Entity object of the entity identification fields
     *
     *  @param[in] key - the entity identification fields of the PDR
     *
     *  @return entity object
     */
    std::shared_ptr<Entity> getEntity(const EntityKey& key);

    /** @brief Get the State Sensor object of the entity identification fields
     *
     *  @param[in] id - sensor ID
     *
     *  @return state sensor object
     */
    std::shared_ptr<StateSensor> getStateSensorObject(SensorID id);

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

    /** @brief Parse the state sensor PDRs
     *
     *  @param[in] pdrData - the response PDRs from GetPDR command
     *  @return pointer to parsed state sensor info struct
     */
    std::shared_ptr<StateSensorInfo> parseStateSensorPDR(
        const std::vector<uint8_t>& pdrData);

    /** @brief Parse the Entity Association PDRs
     *
     *  @param[in] pdrData - the response PDRs from GetPDR command
     *  @return the containment record of the PDR
     */
    std::optional<EntityAssociation> parseEntityAssociationPDR(
        const std::vector<uint8_t>& pdrData);

    /** @brief Create the D-Bus object of every entity of the terminus and
     *         the containment associations between them. The entity list is
     *         collected from the Entity Association PDRs, the Entity
     *         Auxiliary Names PDRs and the entity identification fields of
     *         the sensor PDRs.
     */
    void addEntities();

    /** @brief Create the D-Bus object of one entity. The entity is not
     *         exposed when its entity type has no matching Inventory.Item
     *         interface.
     *
     *  @param[in] key - the entity identification fields of the PDR
     */
    void addEntity(const EntityKey& key);

    /** @brief Add the containment associations of the Entity Association
     *         PDRs to the entity D-Bus objects
     */
    void addEntityAssociations();

    /** @brief Get the state set interfaces of the D-Bus object of an entity
     *
     *  @param[in] key - the entity identification fields of the PDR
     *  @return the state set interfaces of the entity D-Bus object, nullptr
     *          when no D-Bus object is published for the entity
     */
    std::shared_ptr<StateSets> getEntityStateSets(const EntityKey& key);

    /** @brief Construct the StateSensor class of every state sensor of the
     *         terminus. A state sensor whose entity has no D-Bus object is
     *         not constructed.
     */
    void addStateSensors();

    /** @brief Get the name of an entity
     *
     *  @param[in] key - the entity identification fields of the PDR
     *  @param[in] typeName - the name of the entity type
     *  @return the entity name from the Entity Auxiliary Names PDR, or the
     *          name built from the terminus ID, the entity type name, the
     *          entity instance number and the entity container ID when the
     *          PDR does not name the entity
     */
    std::string getEntityName(const EntityKey& key, std::string_view typeName);

    /** @brief Create the terminus inventory path under
     *         /xyz/openbmc_project/inventory/system/. The concrete
     *         Inventory.Item.* interface is selected from @p entityType.
     *
     *  @param[in] tName - the terminus name
     *  @param[in] entityType - PLDM entity type of the overall terminus
     *                          entity (from the Entity Auxiliary Names PDR
     *                          whose containerId is the system container)
     *  @return true/false: True if there is no error in creating inventory path
     */
    bool createInventoryPath(std::string tName, uint16_t entityType);

    /** @brief Find the identification fields of the overall terminus entity.
     *
     *  Uses the same Entity Auxiliary Names PDR lookup as findTerminusName()
     *  (i.e. the entry whose containerId is the system container).
     *
     *  @return entity identification fields, or nullopt if not found
     */
    std::optional<EntityKey> findTerminusEntityKey();

    /** @brief Find the PLDM entity type of the overall terminus entity.
     *
     *  @return entity type, or 0 if not found
     */
    uint16_t findTerminusEntityType();

    /** @brief Get sensor names from Sensor Auxiliary Names PDRs
     *
     *  @param[in] sensorId - Sensor ID
     *  @param[in] isEffecter - This is an effecter, not a sensor
     *  @return vector of sensor name strings
     *
     */
    std::vector<std::string> getSensorNames(const SensorID& sensorId);

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

    /** @brief Containment records of the Entity Association PDRs */
    std::vector<EntityAssociation> entityAssociations{};

    /** @brief A list of entity D-Bus objects */
    std::vector<std::shared_ptr<Entity>> entities{};

    /** @brief The state set interfaces of the terminus inventory path, on
     *         which the state sensors of the overall terminus entity publish
     */
    std::shared_ptr<StateSets> terminusStateSets{};

    /** @brief Terminus name */
    EntityName terminusName{};
    /* @brief The pointer of inventory D-Bus interface for the terminus */
    std::unique_ptr<pldm::dbus_api::PldmEntityBase> inventoryItemInft = nullptr;

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

    /** @brief State Sensor PDR list */
    std::vector<std::shared_ptr<StateSensorInfo>> stateSensorPdrs{};

    /** @brief Iteration to loop through sensor PDRs when adding sensors */
    SensorID sensorPdrIt = 0;
};
} // namespace platform_mc
} // namespace pldm
