#pragma once

#include "../common/types.hpp"
#include "../common/utils.hpp"

#include <libpldm/platform.h>
#include <libpldm/state_set.h>

#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace pldm
{
namespace platform_mc
{

using Level = sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level;
using AssociationDefinitionsIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;
using Associations =
    std::vector<std::tuple<std::string, std::string, std::string>>;

/**
 * @brief Abstract base class for PLDM State Sets (DSP0249)
 *
 * This class provides the interface for all state set implementations.
 * Each state set from DSP0249 should inherit from this class.
 */
class StateSet
{
  public:
    /**
     * @brief Construct a StateSet
     * @param[in] stateSetId - State set ID from DSP0249
     */
    explicit StateSet(uint16_t stateSetId) : stateSetId(stateSetId) {}

    virtual ~StateSet() = default;

    /**
     * @brief Set the state value
     * @param[in] value - State value to set
     */
    virtual void setValue(uint8_t value) = 0;

    /**
     * @brief Set the default/initial state value
     */
    virtual void setDefaultValue() = 0;

    /**
     * @brief Get the current state value
     * @return uint8_t - Current state value
     */
    virtual uint8_t getValue() const
    {
        return currentValue;
    }

    /**
     * @brief Get the state set ID
     * @return uint16_t - State set ID from DSP0249
     */
    uint16_t getStateSetId() const
    {
        return stateSetId;
    }

    /**
     * @brief Get human-readable state type name
     * @return std::string - State type name (e.g., "Health", "Availability")
     */
    virtual std::string getStateTypeName() const = 0;

    /**
     * @brief Get human-readable state value name
     * @return std::string - State value name (e.g., "Normal", "Critical")
     */
    virtual std::string getStateValueName() const = 0;

    /**
     * @brief Get event data for Redfish/logging
     * @return tuple<messageId, message, level, resolution, additionalData>
     */
    virtual std::tuple<std::string, std::string, Level, std::string,
                       std::string>
        getEventData() const = 0;

    /**
     * @brief Set associations for D-Bus
     * @param[in] associations - Vector of associations
     */
    virtual void setAssociations(const Associations& associations)
    {
        if (associationDefinitionsIntf)
        {
            associationDefinitionsIntf->associations(associations);
        }
    }

    /**
     * @brief Update sensor name (for dynamic naming)
     * @param[in] name - New sensor name
     */
    virtual void updateSensorName([[maybe_unused]] const std::string& name) {}

  protected:
    /** @brief State set ID from DSP0249 */
    uint16_t stateSetId;

    /** @brief Current state value */
    uint8_t currentValue = PLDM_SENSOR_UNKNOWN;

    /** @brief D-Bus association interface */
    std::unique_ptr<AssociationDefinitionsIntf> associationDefinitionsIntf =
        nullptr;
};

/**
 * @brief Factory for creating StateSet objects
 *
 * This factory creates the appropriate StateSet derived class based on
 * the state set ID from DSP0249.
 */
class StateSetFactory
{
  public:
    /**
     * @brief Create a StateSet object
     *
     * @param[in] stateSetId - State set ID from DSP0249
     * @param[in] sensorOffset - Composite sensor offset
     * @param[in] objectPath - D-Bus object path
     * @param[in] associations - D-Bus associations
     *
     * @return std::unique_ptr<StateSet> - Created state set object, or nullptr
     * if unsupported
     */
    static std::unique_ptr<StateSet> create(
        uint16_t stateSetId, uint8_t sensorOffset,
        const std::string& objectPath, const Associations& associations);
};

} // namespace platform_mc
} // namespace pldm
