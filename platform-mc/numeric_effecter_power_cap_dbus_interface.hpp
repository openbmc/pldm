#pragma once

#include "platform-mc/numeric_effecter_dbus_interface.hpp"

#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Control/Power/Cap/server.hpp>

namespace pldm
{
namespace platform_mc
{

using PowerCapIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Control::Power::server::Cap>;

/**
 * @brief NumericEffecterPowerCapHandler
 *
 * Handler that publishes power cap effecter values to D-Bus using the
 * xyz.openbmc_project.Control.Power.Cap interface. This allows power cap
 * control through standard OpenBMC D-Bus APIs.
 */
class NumericEffecterPowerCapIntf :
    public NumericEffecterDbusInterface,
    public PowerCapIntf
{
  public:
    /**
     * @brief Constructor
     *
     * @param[in] effecter - Reference to the associated NumericEffecter
     * @param[in] bus - D-Bus connection
     * @param[in] path - D-Bus object path
     * @param[in] minValue - Minimum power cap value from PDR (in base units)
     * @param[in] maxValue - Maximum power cap value from PDR (in base units)
     */
    NumericEffecterPowerCapIntf(NumericEffecter& effecter,
                                sdbusplus::bus_t& bus, const std::string& path,
                                double minValue, double maxValue);

    ~NumericEffecterPowerCapIntf() override = default;

    /**
     * @brief Handle numeric effecter value change
     *
     * Updates the D-Bus power cap properties when effecter value changes
     */
    void handleValueChange(NumericEffecter& effecter,
                           pldm_effecter_oper_state operState,
                           double pendingValue, double presentValue) override;

    /**
     * @brief Handle numeric effecter error condition
     *
     * Sets power cap to disabled state on error
     */
    void handleError(NumericEffecter& effecter) override;

    /**
     * @brief Get current power cap value (D-Bus getter)
     *
     * Returns the cached power cap value from D-Bus
     */
    uint32_t powerCap() const override;

    /**
     * @brief Set new power cap value (D-Bus setter)
     *
     * Validates the value and sends SetNumericEffecterValue command to terminus
     *
     * @param[in] value - New power cap value in watts
     * @return uint32_t - The cached power cap value
     */
    uint32_t powerCap(uint32_t value) override;

    /**
     * @brief Get power cap enable state (D-Bus getter)
     *
     * Returns the cached power cap enable state
     */
    bool powerCapEnable() const override;

    /**
     * @brief Set power cap enable state (D-Bus setter)
     *
     * Sends SetNumericEffecterEnable command to terminus
     *
     * @param[in] value - true to enable, false to disable
     * @return bool - The cached enable state
     */
    bool powerCapEnable(bool value) override;

  private:
    /** @brief Reference to associated NumericEffecter */
    NumericEffecter& effecter;
};

} // namespace platform_mc
} // namespace pldm
