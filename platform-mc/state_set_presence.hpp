#pragma once

#include "state_set.hpp"

#include <memory>
#include <string>
#include <utility>

namespace pldm
{
namespace platform_mc
{

/** @class StateSetPresence
 *  @brief The presence state set, state set ID 13 of DSP0249 v1.4.0.
 *  @details The presence of the entity is exposed by the Present property of
 *           Inventory.Item, the interface the entity D-Bus object implements.
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
     *  @param[in] path - D-Bus object path of the entity
     *  @param[in] itemIntf - the Inventory.Item interface of the entity
     */
    StateSetPresence(const std::string& path,
                     std::shared_ptr<InventoryItemServer> itemIntf) :
        path(path), interface(std::move(itemIntf))
    {}

    void setPresentState(uint8_t presentState) override;

    /** @brief The getter to return the presence the interface carries */
    bool present() const
    {
        return interface->present();
    }

  private:
    /** @brief The D-Bus object path of the entity */
    std::string path;

    /** @brief The interface which carries the presence of the entity */
    std::shared_ptr<InventoryItemServer> interface;

    /** @brief Whether a state the state set does not define was logged */
    bool unknownStateLogged = false;
};

} // namespace platform_mc
} // namespace pldm
