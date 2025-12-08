#include "platform-mc/numeric_effecter_power_cap_dbus_interface.hpp"

#include "platform-mc/numeric_effecter.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

namespace pldm
{
namespace platform_mc
{

PHOSPHOR_LOG2_USING;

NumericEffecterPowerCapIntf::NumericEffecterPowerCapIntf(
    NumericEffecter& effecter, sdbusplus::bus_t& bus, const std::string& path,
    double minValue, double maxValue) :
    PowerCapIntf(bus, path.c_str()), effecter(effecter)
{
    try
    {
        // Set min/max power cap values from PDR
        PowerCapIntf::maxPowerCapValue(static_cast<uint32_t>(maxValue));
        PowerCapIntf::minPowerCapValue(static_cast<uint32_t>(minValue));
        PowerCapIntf::minSoftPowerCapValue(static_cast<uint32_t>(minValue));

        // Initialize to disabled state
        PowerCapIntf::powerCapEnable(false);
        PowerCapIntf::powerCap(0);

        lg2::info(
            "Created Power Cap interface for effecter {NAME} with min={MIN}W max={MAX}W",
            "NAME", effecter.name, "MIN", minValue, "MAX", maxValue);
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to initialize Power Cap D-Bus interface for {PATH}: {ERROR}",
            "PATH", path, "ERROR", e);
        throw;
    }
}

void NumericEffecterPowerCapIntf::handleValueChange(
    [[maybe_unused]] NumericEffecter& effecter,
    pldm_effecter_oper_state operState, double pendingValue,
    double presentValue)
{
    double value;
    bool enabled;

    switch (operState)
    {
        case EFFECTER_OPER_STATE_ENABLED_UPDATEPENDING:
            value = pendingValue;
            enabled = true;
            break;
        case EFFECTER_OPER_STATE_ENABLED_NOUPDATEPENDING:
            value = presentValue;
            enabled = true;
            break;
        case EFFECTER_OPER_STATE_DISABLED:
        case EFFECTER_OPER_STATE_INITIALIZING:
        case EFFECTER_OPER_STATE_UNAVAILABLE:
        case EFFECTER_OPER_STATE_STATUSUNKNOWN:
        case EFFECTER_OPER_STATE_FAILED:
        case EFFECTER_OPER_STATE_SHUTTINGDOWN:
        case EFFECTER_OPER_STATE_INTEST:
        default:
            value = 0;
            enabled = false;
            break;
    }

    try
    {
        // Update D-Bus properties (signal=false to avoid triggering property
        // change signals)
        PowerCapIntf::powerCapEnable(enabled, false);
        if (enabled)
        {
            PowerCapIntf::powerCap(static_cast<uint32_t>(value), false);
            lg2::debug(
                "Updated power cap for {NAME}: value={VALUE}W enabled={ENABLED}",
                "NAME", effecter.name, "VALUE", value, "ENABLED", enabled);
        }
        else
        {
            lg2::debug("Disabled power cap for {NAME}", "NAME", effecter.name);
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to update Power Cap D-Bus properties for {NAME}: {ERROR}",
            "NAME", effecter.name, "ERROR", e);
    }
}

void NumericEffecterPowerCapIntf::handleError(
    [[maybe_unused]] NumericEffecter& effecter)
{
    try
    {
        // On error, set to disabled state
        PowerCapIntf::powerCapEnable(false, false);
        PowerCapIntf::powerCap(0, false);
        lg2::debug("Set power cap to disabled state due to error for {NAME}",
                   "NAME", effecter.name);
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to update Power Cap D-Bus properties on error for {NAME}: {ERROR}",
            "NAME", effecter.name, "ERROR", e);
    }
}

uint32_t NumericEffecterPowerCapIntf::powerCap() const
{
    return PowerCapIntf::powerCap();
}

uint32_t NumericEffecterPowerCapIntf::powerCap(uint32_t value)
{
    // Validate against min/max from PDR
    if (value > PowerCapIntf::maxPowerCapValue() ||
        value < PowerCapIntf::minPowerCapValue())
    {
        lg2::error(
            "Power cap value {VALUE}W out of range [{MIN}W, {MAX}W] for {NAME}",
            "VALUE", value, "MIN", PowerCapIntf::minPowerCapValue(), "MAX",
            PowerCapIntf::maxPowerCapValue(), "NAME", effecter.name);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    lg2::info("Setting power cap for {NAME} to {VALUE}W", "NAME", effecter.name,
              "VALUE", value);

    // Convert to raw value and send to terminus
    double baseValue = static_cast<double>(value);
    double rawValue = effecter.baseToRaw(baseValue);

    // Spawn detached coroutine to send the command asynchronously
    scope.spawn(stdexec::just() |
                    stdexec::let_value([&, rawValue]() -> exec::task<void> {
                        co_await effecter.setNumericEffecterValue(rawValue);
                    }),
                exec::default_task_context<void>(stdexec::inline_scheduler{}));

    // Return current cached value (will be updated when response is received)
    return PowerCapIntf::powerCap();
}

bool NumericEffecterPowerCapIntf::powerCapEnable() const
{
    return PowerCapIntf::powerCapEnable();
}

bool NumericEffecterPowerCapIntf::powerCapEnable(bool value)
{
    pldm_effecter_oper_state newState;
    if (value)
    {
        newState = EFFECTER_OPER_STATE_ENABLED_UPDATEPENDING;
        lg2::info("Enabling power cap for {NAME}", "NAME", effecter.name);
    }
    else
    {
        newState = EFFECTER_OPER_STATE_DISABLED;
        lg2::info("Disabling power cap for {NAME}", "NAME", effecter.name);
    }

    // Spawn detached coroutine to send the command asynchronously
    scope.spawn(stdexec::just() |
                    stdexec::let_value([&, newState]() -> exec::task<void> {
                        co_await effecter.setNumericEffecterEnable(newState);
                    }),
                exec::default_task_context<void>(stdexec::inline_scheduler{}));

    // Return current cached value (will be updated when response is received)
    return PowerCapIntf::powerCapEnable();
}

} // namespace platform_mc
} // namespace pldm
