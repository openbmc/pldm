#pragma once

#include "common/types.hpp"

#include <libpldm/state_set.h>

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <algorithm>
#include <array>
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

using StateSetCreator = std::unique_ptr<StateSetBase> (*)(sdbusplus::bus_t&,
                                                          const std::string&);

/** @struct StateSetItem
 *  @brief The D-Bus interface of one state set.
 */
struct StateSetItem
{
    StateSetId stateSetId;  //!< DSP0249 state set ID
    StateSetCreator create; //!< Creator of the D-Bus interface
};

using OperationalStatusIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::State::
                                    Decorator::server::OperationalStatus>;

/** @class StateSetHealthState
 *  @brief The health state set, state set ID 1 of DSP0249 v1.4.0.
 *  @details The health of the entity is exposed by the Functional property of
 *           State.Decorator.OperationalStatus.
 */
class StateSetHealthState : public StateSetBase
{
  public:
    StateSetHealthState() = delete;
    StateSetHealthState(const StateSetHealthState&) = delete;
    StateSetHealthState& operator=(const StateSetHealthState&) = delete;
    StateSetHealthState(StateSetHealthState&&) = delete;
    StateSetHealthState& operator=(StateSetHealthState&&) = delete;
    ~StateSetHealthState() override = default;

    /** @brief Constructor
     *
     *  @param[in] bus - D-Bus bus
     *  @param[in] path - D-Bus object path of the entity
     */
    StateSetHealthState(sdbusplus::bus_t& bus, const std::string& path) :
        interface(bus, path.c_str()), path(path)
    {}

    /** @brief Creator of the interface, `StateSetCreator` of the state set */
    static std::unique_ptr<StateSetBase> create(sdbusplus::bus_t& bus,
                                                const std::string& path)
    {
        return std::make_unique<StateSetHealthState>(bus, path);
    }

    void setPresentState(uint8_t presentState) override;

    /** @brief The getter to return the health the interface carries */
    bool functional() const
    {
        return interface.functional();
    }

  private:
    /** @brief The interface which carries the health of the entity */
    OperationalStatusIntf interface;

    /** @brief The D-Bus object path of the entity */
    std::string path;

    /** @brief Whether a state the state set does not define was logged */
    bool unknownStateLogged = false;
};

/** @brief The state sets which have a D-Bus interface.
 *
 *  The mapping is injective: two state sets do not share the property of a
 *  D-Bus interface, so the component sensors of one entity do not overwrite
 *  each other. A state set gets its entry when its interface is added.
 */
inline constexpr std::array<StateSetItem, 1> stateSetItems{
    StateSetItem{PLDM_STATE_SET_HEALTH_STATE, &StateSetHealthState::create},
};

/** @brief Create the D-Bus interface which matches the given state set
 *  @param[in] bus - D-Bus bus
 *  @param[in] path - D-Bus object path
 *  @param[in] stateSetId - DSP0249 state set ID
 *  @return unique_ptr to StateSetBase, nullptr when the state set has no
 *          matching D-Bus interface
 */
inline std::unique_ptr<StateSetBase> createStateSet(
    sdbusplus::bus_t& bus, const std::string& path, StateSetId stateSetId)
{
    auto it =
        std::ranges::find(stateSetItems, stateSetId, &StateSetItem::stateSetId);
    if (it == stateSetItems.end())
    {
        return nullptr;
    }
    return it->create(bus, path);
}

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
