#pragma once

#include "state_set.hpp"

#include <xyz/openbmc_project/Control/Power/Throttle/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Degradation/server.hpp>

#include <optional>
#include <string>
#include <vector>

namespace pldm
{
namespace platform_mc
{

using DegradationIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::State::Decorator::server::Degradation>;
using ThrottleIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Control::Power::server::Throttle>;
using ThrottleReasons = sdbusplus::xyz::openbmc_project::Control::Power::
    server::Throttle::ThrottleReasons;

/** @class StateSetPerformance
 *  @brief The performance state set, state set ID 14 of DSP0249 v1.4.0.
 *  @details A throttled entity is exposed by the Throttled property of
 *           Control.Power.Throttle, and a degraded entity by the Degraded
 *           property of State.Decorator.Degradation.
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
     *  @param[in] stateSets - the state set interfaces of the D-Bus object of
     *                         the entity
     */
    explicit StateSetPerformance(StateSets& stateSets) :
        degradation(stateSets.getInterface<DegradationIntf>()),
        throttle(stateSets.getInterface<ThrottleIntf>()),
        path(stateSets.getPath())
    {}

    void setPresentState(uint8_t presentState) override;

    /** @brief The getter to return whether the entity is degraded */
    bool degraded() const
    {
        return degradation.degraded();
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
    /** @brief Set whether the entity is throttled, together with its causes
     *
     *  @param[in] throttled - whether the entity is throttled
     */
    void setThrottled(bool throttled);

    /** @brief The interface which carries the degraded entity, shared with
     *         the other state sets of the entity
     */
    DegradationIntf& degradation;

    /** @brief The interface which carries the throttled entity, shared with
     *         the other state sets of the entity
     */
    ThrottleIntf& throttle;

    /** @brief The D-Bus object path of the entity */
    std::string path;

    /** @brief The last state the state set does not define which was logged */
    std::optional<uint8_t> lastUnknownState;
};

} // namespace platform_mc
} // namespace pldm
