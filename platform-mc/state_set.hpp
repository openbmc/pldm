#pragma once

#include "common/types.hpp"

#include <libpldm/state_set.h>

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Control/Power/Throttle/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Performance/server.hpp>

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

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

using PerformanceIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::State::Decorator::server::Performance>;
using ThrottleIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Control::Power::server::Throttle>;
using ThrottleReasons = sdbusplus::xyz::openbmc_project::Control::Power::
    server::Throttle::ThrottleReasons;

/** @class StateSetPerformance
 *  @brief The performance state set, state set ID 14 of DSP0249 v1.4.0.
 *  @details A throttled entity is exposed by the Throttled property of
 *           Control.Power.Throttle, and a degraded entity by the Degraded
 *           property of State.Decorator.Performance.
 */
class StateSetPerformance : public StateSetBase
{
  public:
    StateSetPerformance() = delete;
    StateSetPerformance(const StateSetPerformance&) = delete;
    StateSetPerformance& operator=(const StateSetPerformance&) = delete;
    StateSetPerformance(StateSetPerformance&&) = delete;
    StateSetPerformance& operator=(StateSetPerformance&&) = delete;
    ~StateSetPerformance() override = default;

    /** @brief Constructor
     *
     *  @param[in] bus - D-Bus bus
     *  @param[in] path - D-Bus object path of the entity
     */
    StateSetPerformance(sdbusplus::bus_t& bus, const std::string& path) :
        performance(bus, path.c_str()), throttle(bus, path.c_str()), path(path)
    {}

    /** @brief Creator of the interface, `StateSetCreator` of the state set */
    static std::unique_ptr<StateSetBase> create(sdbusplus::bus_t& bus,
                                                const std::string& path)
    {
        return std::make_unique<StateSetPerformance>(bus, path);
    }

    void setPresentState(uint8_t presentState) override;

    /** @brief The getter to return whether the entity is degraded */
    bool degraded() const
    {
        return performance.degraded();
    }

    /** @brief The getter to return whether the entity is throttled */
    bool throttled() const
    {
        return throttle.throttled();
    }

    /** @brief The getter to return the causes of the throttling */
    std::vector<ThrottleReasons> throttleCauses() const
    {
        return throttle.throttleCauses();
    }

  private:
    /** @brief The interface which carries the degraded entity */
    PerformanceIntf performance;

    /** @brief The interface which carries the throttled entity */
    ThrottleIntf throttle;

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
    StateSetItem{PLDM_STATE_SET_PERFORMANCE, &StateSetPerformance::create},
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
