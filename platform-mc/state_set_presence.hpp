#pragma once

#include "state_set.hpp"

#include <xyz/openbmc_project/Inventory/Item/server.hpp>

#include <string>

namespace pldm
{
namespace platform_mc
{

using InventoryItemIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::server::Item>;

/** @class StateSetPresence
 *  @brief The presence state set, state set ID 13 of DSP0249 v1.4.0.
 *  @details The presence of the entity is exposed by the Present property of
 *           Inventory.Item, the interface the entity D-Bus object already
 *           implements.
 */
class StateSetPresence : public StateSetBase
{
  public:
    StateSetPresence() = delete;
    StateSetPresence(const StateSetPresence&) = delete;
    StateSetPresence& operator=(const StateSetPresence&) = delete;
    StateSetPresence(StateSetPresence&&) = delete;
    StateSetPresence& operator=(StateSetPresence&&) = delete;
    ~StateSetPresence() override = default;

    /** @brief Constructor
     *
     *  @param[in] stateSets - the state set interfaces of the entity D-Bus
     *                         object, which hold its Inventory.Item interface
     */
    explicit StateSetPresence(StateSets& stateSets) :
        path(stateSets.getPath()),
        interface(stateSets.getInterface<InventoryItemIntf>())
    {}

    void setPresentState(uint8_t presentState) override;

    /** @brief The getter to return the presence the interface carries */
    bool present() const
    {
        return interface.present();
    }

  private:
    /** @brief The D-Bus object path of the entity */
    std::string path;

    /** @brief The interface which carries the presence of the entity */
    InventoryItemIntf& interface;

    /** @brief Whether a state the state set does not define was logged */
    bool unknownStateLogged = false;
};

} // namespace platform_mc
} // namespace pldm
