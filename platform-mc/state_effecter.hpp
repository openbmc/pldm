#pragma once

#include "common/types.hpp"
#include "state_set.hpp"

#include <libpldm/platform.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace pldm
{
namespace platform_mc
{

using namespace pldm::pdr;

/** @struct StateEffecterInfo
 *
 *  The parsed State Effecter PDR: the fixed fields plus, for each composite
 *  effecter offset, the state set ID and the possible state values parsed from
 *  the possible_states[] region.
 */
struct StateEffecterInfo
{
    /** @brief Fixed portion of the State Effecter PDR, up to and including
     *         compositeEffecterCount
     */
    pldm_platform_state_effecter_pdr pdr;

    /** @brief State set ID and possible state values of each composite
     *         effecter, indexed by its effecter offset
     */
    std::vector<std::pair<StateSetId, PossibleStates>> componentInfo;
};

/**
 * @brief StateEffecter
 *
 * One state effecter of a terminus. A state effecter has no D-Bus object of
 * its own: each of its composite effecters sets the state of the entity which
 * the State Effecter PDR identifies, through the writable state set property
 * of that entity's D-Bus object. An effecter offset whose state set has no
 * writable property, or whose state set another state effecter of the entity
 * already controls, is not exposed.
 */
class StateEffecter
{
  public:
    StateEffecter() = delete;
    StateEffecter(const StateEffecter&) = delete;
    StateEffecter& operator=(const StateEffecter&) = delete;
    StateEffecter(StateEffecter&&) = delete;
    StateEffecter& operator=(StateEffecter&&) = delete;
    ~StateEffecter() = default;

    /** @brief Constructor
     *
     *  Records the state set of each exposed effecter offset as controlled
     *  by a state effecter in @p stateSets.
     *
     *  @param[in] tid - terminus ID of the effecter
     *  @param[in] info - the parsed State Effecter PDR
     *  @param[in] stateSets - the state set interfaces of the D-Bus object of
     *                         the entity which the PDR identifies
     */
    StateEffecter(pldm_tid_t tid, std::shared_ptr<StateEffecterInfo> info,
                  std::shared_ptr<StateSets> stateSets);

    /** @brief The getter to return the terminus ID of the effecter */
    pldm_tid_t getTid() const
    {
        return tid;
    }

    /** @brief The getter to return the effecter ID */
    EffecterID getEffecterId() const
    {
        return info->pdr.effecter_id;
    }

    /** @brief The getter to return the number of composite effecters */
    uint8_t getCompositeEffecterCount() const
    {
        return info->pdr.composite_effecter_count;
    }

    /** @brief Whether any effecter offset of the effecter is exposed */
    bool hasExposedOffset() const;

    /** @brief Whether the initialization agent has to enable the composite
     *         effecters, `effecterInit` of `Table 90 - State Effecter PDR`
     *         of DSP0248 v1.3.0
     */
    bool requiresEnable() const
    {
        return info->pdr.effecter_init == PLDM_ENABLE_EFFECTER;
    }

    /** @brief Whether SetStateEffecterEnables has to be sent for the
     *         composite effecters of the effecter
     */
    enum class EnableState : uint8_t
    {
        pending,  /**< the command has not completed for the effecter */
        enabled,  /**< the terminus enabled the composite effecters */
        rejected, /**< the terminus answered with an error completion code,
                       so the command is not sent for the effecter again */
    };

    /** @brief Outcome of SetStateEffecterEnables for the composite effecters
     *         of the effecter
     */
    EnableState enableState = EnableState::pending;

    /** @brief Publish the state which one composite effecter reports on the
     *         state set property of the entity
     *  @details `Table 54 - GetStateEffecterStates stateField format` of
     *           DSP0248 v1.3.0 makes presentState meaningful only while the
     *           composite effecter is enabled, so the property is kept for
     *           any other operational state, and for a state which is not
     *           one of the possible states of the composite effecter.
     *
     *  @param[in] offset - effecter offset of the composite effecter
     *  @param[in] opState - the effecterOperationalState of
     *                       GetStateEffecterStates
     *  @param[in] presentState - the presentState of GetStateEffecterStates
     */
    void updatePresentState(uint8_t offset, uint8_t opState,
                            uint8_t presentState);

    /** @brief Whether the getter of the effecter is active, so that
     *         GetStateEffecterStates is sent on the next polling pass of the
     *         terminus. The getter starts active after discovery, and stops
     *         when no composite effecter reports enabled-updatePending or
     *         when GetStateEffecterStates fails.
     */
    bool getterActive = true;

  private:
    /** @brief Terminus ID which the effecter belongs to */
    pldm_tid_t tid;

    /** @brief The parsed State Effecter PDR of the effecter */
    std::shared_ptr<StateEffecterInfo> info;

    /** @brief The state set interfaces of the D-Bus object of the entity.
     *         The effecter shares the ownership so that the interfaces it
     *         points to outlive it.
     */
    std::shared_ptr<StateSets> stateSets;

    /** @brief The state set interface of each composite effecter, indexed by
     *         its effecter offset, nullptr when the effecter offset is not
     *         exposed. The interfaces are owned by stateSets.
     */
    std::vector<StateSetBase*> componentStateSets;
};

} // namespace platform_mc
} // namespace pldm
