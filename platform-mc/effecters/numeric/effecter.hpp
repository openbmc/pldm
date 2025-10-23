#pragma once

#include "common/types.hpp"
#include "platform-mc/effecters/numeric/dbus_intf.hpp"

#include <libpldm/platform.h>

#include <sdbusplus/async.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>

#include <memory>
#include <vector>

namespace pldm
{
namespace platform_mc
{

class TerminusManager;
class NumericEffecterDbusInterface;

using Associations =
    std::vector<std::tuple<std::string, std::string, std::string>>;
using AssociationDefinitionsInft = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

/**
 * @brief NumericEffecter
 *
 * This class handles effecter value setting and exports
 * status to D-Bus interface.
 */
class NumericEffecter
{
  public:
    NumericEffecter(const pldm_tid_t tid,
                    std::shared_ptr<pldm_numeric_effecter_value_pdr> pdr,
                    const std::string& effecterName,
                    const std::string& associationPath,
                    TerminusManager& terminusManager);
    ~NumericEffecter() {};

    /** @brief The function called to set effecter to
     * error status.
     */
    void handleErrGetNumericEffecterValue();

    /** @brief Updating the effecter status to D-Bus interface
     */
    void updateValue(pldm_effecter_oper_state effecterOperState,
                     double pendingValue, double presentValue);

    /** @brief Sending setNumericEffecterEnable command for the effecter
     *
     *  @param[in] state - the effecter state to be set
     */
    exec::task<int> setNumericEffecterEnable(pldm_effecter_oper_state state);

    /** @brief Sending setNumericEffecterValue command for the effecter
     *
     *  @param[in] effecterValue - the effecter value to be set
     */
    exec::task<int> setNumericEffecterValue(double effecterValue);

    /** @brief Sending getNumericEffecterValue command for the effecter
     */
    exec::task<int> getNumericEffecterValue();

    /** @brief Get the ContainerID, EntityType, EntityInstance of the PLDM
     * Entity which the effecter belongs to
     *  @return EntityInfo - Entity ID
     */
    inline auto getEntityInfo()
    {
        return entityInfo;
    }

    /** @brief Updating the association to D-Bus interface
     *  @param[in] inventoryPath - inventory path of the entity
     */
    inline void setInventoryPaths(const std::vector<std::string>& inventoryPath)
    {
        if (associationDefinitionsIntf)
        {
            std::map<std::pair<std::string, std::string>, bool> assocMap;
            Associations assocs{};

            auto associations = associationDefinitionsIntf->associations();
            for (auto& association : associations)
            {
                auto& [forward, reverse, objectPath] = association;
                auto iter = assocMap.find(std::make_pair(forward, reverse));
                if (iter == assocMap.end())
                {
                    for (const auto& path : inventoryPath)
                    {
                        assocs.emplace_back(std::make_tuple(
                            forward.c_str(), reverse.c_str(), path.c_str()));
                    }
                    assocMap[{forward, reverse}] = true;
                }
            }
            associationDefinitionsIntf->associations(assocs);
        }
    }

    /** @brief getter of baseUnit member variable */
    uint8_t getBaseUnit()
    {
        return baseUnit;
    }

    /** @brief get the association */
    auto getAssociation() const
    {
        return associationDefinitionsIntf->associations();
    }

    /** @brief Set effecter unit and namespace based on baseUnit
     *
     *  @param[in] baseUnit - The PLDM defined baseUnit enum
     */
    void setEffecterUnit(uint8_t baseUnit);

    /** @brief Convert raw value to effecter unit value
     *  Raw value is the value stored in the PDR.
     *  Conversion: effecterUnit = raw * resolution + offset
     *
     *  @param[in] value - raw value
     *  @return double - converted effecter unit value
     */
    double rawToUnit(double value);

    /** @brief Convert effecter unit value to raw value
     *  Conversion: raw = (effecterUnit - offset) / resolution
     *
     *  @param[in] value - effecter unit value
     *  @return double - converted raw value
     */
    double unitToRaw(double value);

    /** @brief Convert effecter unit value to base unit value
     *  Conversion: baseUnit = effecterUnit * 10^unitModifier
     *
     *  @param[in] value - effecter unit value
     *  @return double - converted base unit value
     */
    double unitToBase(double value);

    /** @brief Convert base unit value to effecter unit value
     *  Conversion: effecterUnit = baseUnit * 10^(-unitModifier)
     *
     *  @param[in] value - base unit value
     *  @return double - converted effecter unit value
     */
    double baseToUnit(double value);

    /** @brief Convert raw value to base unit value
     *  Convenience method: baseUnit = unitToBase(rawToUnit(raw))
     *
     *  @param[in] value - raw value
     *  @return double - converted base unit value
     */
    inline double rawToBase(double value)
    {
        return unitToBase(rawToUnit(value));
    }

    /** @brief Convert base unit value to raw value
     *  Convenience method: raw = unitToRaw(baseToUnit(baseUnit))
     *
     *  @param[in] value - base unit value
     *  @return double - converted raw value
     */
    inline double baseToRaw(double value)
    {
        return unitToRaw(baseToUnit(value));
    }

    /** @brief Register a interface for effecter value changes
     *
     *  @param[in] interface - Unique pointer to the interface interface
     */
    void registerInterface(std::unique_ptr<NumericEffecterDbusInterface> intf);

    /** @brief Effecter name */
    std::string name;

    /** @brief  The DBus path of effecter */
    std::string path;

    /** @brief ContainerID, EntityType, EntityInstance of the PLDM Entity
     * which the effecter belongs to */
    pldm::pdr::EntityInfo entityInfo;

    /** @brief Effecter ID */
    uint16_t effecterId;

    /** @brief Terminus ID which the effecter belongs to */
    pldm_tid_t tid;

    /** @brief  The PLDM defined effecterDataSize enum */
    uint8_t dataSize;

    /** @brief flag to update the value once */
    bool needsUpdate;

    static constexpr const std::string reverseAssociation = "all_controls";

  private:
    std::unique_ptr<AssociationDefinitionsInft> associationDefinitionsIntf =
        nullptr;

    /** @brief The resolution of effecter in Units */
    double resolution;

    /** @brief A constant value that is added in as part of conversion
     * process of converting a raw effecter reading to Units */
    double offset;

    /** @brief reference of TerminusManager */
    TerminusManager& terminusManager;

    std::shared_ptr<pldm_numeric_effecter_value_pdr> pdr;

    /** @brief Registered interfaces for value changes */
    std::vector<std::unique_ptr<NumericEffecterDbusInterface>> interfaces;

    /** @brief The DBus namespace for effecter based on baseUnit */
    std::string effecterNameSpace;

    /** @brief A power-of-10 multiplier for baseUnit */
    int8_t unitModifier;

    /** @brief baseUnit of numeric effecter */
    uint8_t baseUnit;
};
} // namespace platform_mc
} // namespace pldm
