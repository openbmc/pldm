#pragma once

#include "common/types.hpp"

#include <sdbusplus/bus.hpp>

#include <map>
#include <memory>
#include <string>

namespace pldm
{
namespace platform_mc
{

using namespace pldm::pdr;

/** @class StateSetBase
 *  @brief Abstract base of the D-Bus interface of one state set.
 *  @details The interface is implemented on the D-Bus object of the entity
 *           whose state the component sensors of the state set report.
 */
class StateSetBase
{
  public:
    StateSetBase() = default;
    StateSetBase(const StateSetBase&) = delete;
    StateSetBase& operator=(const StateSetBase&) = delete;
    StateSetBase(StateSetBase&&) = delete;
    StateSetBase& operator=(StateSetBase&&) = delete;
    virtual ~StateSetBase() = default;

    /** @brief Set the property of the interface from the state which a
     *         component sensor of the state set reports
     *
     *  @param[in] presentState - the presentState of GetStateSensorReadings
     */
    virtual void setPresentState(uint8_t presentState) = 0;
};

/** @brief Create the D-Bus interface which matches the given state set
 *
 *  @param[in] bus - D-Bus bus
 *  @param[in] path - D-Bus object path
 *  @param[in] stateSetId - DSP0249 state set ID
 *  @return unique_ptr to StateSetBase, nullptr when the state set has no
 *          matching D-Bus interface
 */
std::unique_ptr<StateSetBase> createStateSet(
    sdbusplus::bus_t& bus, const std::string& path, StateSetId stateSetId);

/** @class StateSets
 *  @brief The state set interfaces implemented on one D-Bus object.
 *  @details The component sensors which report the same state set of the same
 *           entity share one interface, so the interface of a state set is
 *           created once and then looked up by its state set ID.
 */
class StateSets
{
  public:
    StateSets() = delete;
    StateSets(const StateSets&) = delete;
    StateSets& operator=(const StateSets&) = delete;
    StateSets(StateSets&&) = delete;
    StateSets& operator=(StateSets&&) = delete;
    ~StateSets() = default;

    /** @brief Constructor
     *
     *  @param[in] path - the D-Bus object path the interfaces are
     *                    implemented on
     */
    explicit StateSets(const std::string& path) : path(path) {}

    /** @brief Get the D-Bus interface of the state set, implementing it on
     *         the D-Bus object when it is not implemented yet
     *
     *  @param[in] stateSetId - DSP0249 state set ID
     *  @return the interface of the state set, nullptr when the state set has
     *          no D-Bus interface
     */
    StateSetBase* getStateSet(StateSetId stateSetId);

  private:
    /** @brief The D-Bus object path the interfaces are implemented on */
    std::string path;

    /** @brief The interface of each implemented state set */
    std::map<StateSetId, std::unique_ptr<StateSetBase>> stateSets;
};

} // namespace platform_mc
} // namespace pldm
