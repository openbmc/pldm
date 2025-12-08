#pragma once

#include <libpldm/platform.h>

namespace pldm
{
namespace platform_mc
{

class NumericEffecter;

/**
 * @brief NumericEffecterDbusInterface
 *
 * Interface for handlers that respond to numeric effecter value changes.
 * Implementations of this interface can be registered with a NumericEffecter
 * to receive notifications when the effecter's value or state changes.
 */
class NumericEffecterDbusInterface
{
  public:
    virtual ~NumericEffecterDbusInterface() = default;

    /**
     * @brief Handle numeric effecter value change
     *
     * Called when the effecter's value or operational state changes.
     * Implementations should handle the change appropriately (e.g., update
     * D-Bus properties, trigger side effects, validate constraints, etc.)
     *
     * @param[in] effecter - Reference to the effecter that changed
     * @param[in] operState - New operational state of the effecter
     * @param[in] pendingValue - Pending value if update is pending
     * @param[in] presentValue - Current present value of the effecter
     */
    virtual void handleValueChange(
        NumericEffecter& effecter, pldm_effecter_oper_state operState,
        double pendingValue, double presentValue) = 0;

    /**
     * @brief Handle numeric effecter error condition
     *
     * Called when an error occurs while reading the effecter value.
     * Implementations should handle the error appropriately (e.g., set
     * error states, log errors, etc.)
     *
     * @param[in] effecter - Reference to the effecter in error state
     */
    virtual void handleError(NumericEffecter& effecter) = 0;
};

} // namespace platform_mc
} // namespace pldm
