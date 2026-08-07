#pragma once

#include "state_set.hpp"

#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <string>

namespace pldm
{
namespace platform_mc
{

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

} // namespace platform_mc
} // namespace pldm
