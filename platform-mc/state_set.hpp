#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"

#include <sdbusplus/bus.hpp>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

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

class StateSets;

/** @brief Create the D-Bus interface which matches the given state set
 *
 *  Two state sets may share a D-Bus interface, but not a property of it, so
 *  the component sensors of one entity do not overwrite each other. A state
 *  set gets its case when its interface is added.
 *
 *  @param[in] stateSets - the state set interfaces of the D-Bus object
 *  @param[in] stateSetId - DSP0249 state set ID
 *  @return unique_ptr to StateSetBase, nullptr when the state set has no
 *          matching D-Bus interface
 */
std::unique_ptr<StateSetBase> createStateSet(StateSets& stateSets,
                                             StateSetId stateSetId);

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

    /** @brief The getter to return the D-Bus object path the interfaces are
     *         implemented on
     */
    const std::string& getPath() const
    {
        return path;
    }

    /** @brief Get a D-Bus interface of the D-Bus object, implementing it when
     *         it is not implemented yet
     *
     *  The state sets which publish on properties of the same D-Bus interface
     *  share one instance of it, as a D-Bus object implements an interface
     *  once.
     *
     *  @tparam Intf - sdbusplus::server::object_t of one D-Bus interface
     *  @param[in] init - sets the initial properties of the interface when
     *                    this call implements it, and is ignored when the
     *                    interface is already implemented
     *  @return the interface
     */
    template <typename Intf>
    Intf& getInterface(const std::function<void(Intf&)>& init = {})
    {
        auto& intf = interfaces[Intf::interface];
        if (!intf)
        {
            auto created = std::make_shared<Intf>(
                pldm::utils::DBusHandler::getBus(), path.c_str());
            if (init)
            {
                init(*created);
            }
            intf = std::move(created);
        }
        return *std::static_pointer_cast<Intf>(intf);
    }

    /** @brief Add a D-Bus interface which the D-Bus object already implements
     *
     *  The state sets which publish on the interface then share it instead of
     *  implementing it a second time.
     *
     *  @tparam Intf - sdbusplus::server::object_t of one D-Bus interface
     *  @param[in] intf - the interface, which the state sets keep alive
     */
    template <typename Intf>
    void addInterface(std::shared_ptr<Intf> intf)
    {
        interfaces[Intf::interface] = std::move(intf);
    }

    /** @brief Find a D-Bus interface of the D-Bus object, without implementing
     *         it
     *
     *  @tparam Intf - sdbusplus::server::object_t of one D-Bus interface
     *  @return the interface, nullptr when the D-Bus object does not implement
     *          it
     */
    template <typename Intf>
    Intf* findInterface()
    {
        auto it = interfaces.find(Intf::interface);
        if (it == interfaces.end() || !it->second)
        {
            return nullptr;
        }
        return static_cast<Intf*>(it->second.get());
    }

  private:
    /** @brief The D-Bus object path the interfaces are implemented on */
    std::string path;

    /** @brief The D-Bus interfaces implemented on the D-Bus object, keyed by
     *         interface name. Declared before stateSets, so it outlives the
     *         state sets which refer to the interfaces.
     */
    std::map<std::string_view, std::shared_ptr<void>> interfaces;

    /** @brief The interface of each implemented state set */
    std::map<StateSetId, std::unique_ptr<StateSetBase>> stateSets;
};

} // namespace platform_mc
} // namespace pldm
