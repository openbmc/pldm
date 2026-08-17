#pragma once

#include "state_set.hpp"

#include <memory>
#include <string>

namespace pldm
{
namespace platform_mc
{

using LinkStatusValue = pldm::dbus_api::PortServer::LinkStatus;

/** @class StateSetLinkState
 *  @brief The link state set, state set ID 33 of DSP0249 v1.4.0.
 *  @details The link status of the entity is exposed by the LinkStatus
 *           property of Inventory.Connector.Port, the interface the entity
 *           D-Bus object already implements, which reads Unknown until a
 *           component sensor reports a state.
 */
class StateSetLinkState : public StateSetBase
{
  public:
    StateSetLinkState() = delete;
    StateSetLinkState(const StateSetLinkState&) = delete;
    StateSetLinkState& operator=(const StateSetLinkState&) = delete;
    StateSetLinkState(StateSetLinkState&&) = delete;
    StateSetLinkState& operator=(StateSetLinkState&&) = delete;
    ~StateSetLinkState() override = default;

    /** @brief Constructor
     *
     *  @param[in] path - D-Bus object path of the entity
     *  @param[in] portIntf - the Inventory.Connector.Port interface of the
     *                        entity
     */
    StateSetLinkState(const std::string& path,
                      std::shared_ptr<PortIntf> portIntf) :
        path(path), interface(std::move(portIntf))
    {}

    void setPresentState(uint8_t presentState) override;

    /** @brief The getter to return the link status the interface carries */
    LinkStatusValue linkStatus() const
    {
        return interface->linkStatus();
    }

  private:
    /** @brief The D-Bus object path of the entity */
    std::string path;

    /** @brief The interface which carries the link status of the entity */
    std::shared_ptr<PortIntf> interface;

    /** @brief Whether a state the state set does not define was logged */
    bool unknownStateLogged = false;
};

} // namespace platform_mc
} // namespace pldm
