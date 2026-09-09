#pragma once

#include "state_set.hpp"

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Control/Power/Throttle/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Performance/server.hpp>

#include <optional>
#include <string>
#include <vector>

namespace pldm
{
namespace platform_mc
{

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

    /** @brief The last state the state set does not define which was logged */
    std::optional<uint8_t> lastUnknownState;
};

} // namespace platform_mc
} // namespace pldm
