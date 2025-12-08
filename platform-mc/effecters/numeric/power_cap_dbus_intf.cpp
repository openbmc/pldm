#include "platform-mc/effecters/numeric/power_cap_dbus_intf.hpp"

#include "platform-mc/effecters/numeric/effecter.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cmath>
#include <format>
#include <limits>
#include <stdexcept>

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
    // Casting NaN, negative or too large values to uint32_t is undefined
    auto isValidCapValue = [](double value) {
        return !std::isnan(value) && value >= 0 &&
               value <= std::numeric_limits<uint32_t>::max();
    };
    if (!isValidCapValue(minValue) || !isValidCapValue(maxValue))
    {
        throw std::invalid_argument(
            std::format("Invalid power cap range [{}, {}] for effecter {}",
                        minValue, maxValue, effecter.name));
    }

    try
    {
        // Set min/max power cap values from PDR
        PowerCapIntf::maxPowerCapValue(static_cast<uint32_t>(maxValue));
        PowerCapIntf::minPowerCapValue(static_cast<uint32_t>(minValue));
        PowerCapIntf::minSoftPowerCapValue(static_cast<uint32_t>(minValue));

        // Initialize to disabled state
        PowerCapIntf::powerCapEnable(false);
        PowerCapIntf::powerCap(0);

        info(
            "Created Power Cap interface for effecter {NAME} with min={MIN}W max={MAX}W",
            "NAME", effecter.name, "MIN", minValue, "MAX", maxValue);
    }
    catch (const sdbusplus::exception_t& e)
    {
        error(
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
        PowerCapIntf::powerCapEnable(enabled);
        if (enabled)
        {
            PowerCapIntf::powerCap(static_cast<uint32_t>(value));
            debug(
                "Updated power cap for {NAME}: value={VALUE}W enabled={ENABLED}",
                "NAME", effecter.name, "VALUE", value, "ENABLED", enabled);
        }
        else
        {
            debug("Disabled power cap for {NAME}", "NAME", effecter.name);
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Failed to update Power Cap D-Bus properties for {NAME}: {ERROR}",
              "NAME", effecter.name, "ERROR", e);
    }
}

void NumericEffecterPowerCapIntf::handleError(
    [[maybe_unused]] NumericEffecter& effecter)
{
    // Do Nothing.
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
        error(
            "Power cap value {VALUE}W out of range [{MIN}W, {MAX}W] for {NAME}",
            "VALUE", value, "MIN", PowerCapIntf::minPowerCapValue(), "MAX",
            PowerCapIntf::maxPowerCapValue(), "NAME", effecter.name);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    info("Setting power cap for {NAME} to {VALUE}W", "NAME", effecter.name,
         "VALUE", value);

    // Convert to raw value and send to terminus
    double baseValue = static_cast<double>(value);
    double rawValue = effecter.baseToRaw(baseValue);

    // Spawn detached coroutine to send the command asynchronously
    scope.spawn(
        [](NumericEffecter& effecter, double rawValue) -> exec::task<void> {
            co_await effecter.setNumericEffecterValue(rawValue);
        }(effecter, rawValue),
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
        info("Enabling power cap for {NAME}", "NAME", effecter.name);
    }
    else
    {
        newState = EFFECTER_OPER_STATE_DISABLED;
        info("Disabling power cap for {NAME}", "NAME", effecter.name);
    }

    // Spawn detached coroutine to send the command asynchronously
    scope.spawn(
        [](NumericEffecter& effecter,
           pldm_effecter_oper_state newState) -> exec::task<void> {
            co_await effecter.setNumericEffecterEnable(newState);
        }(effecter, newState),
        exec::default_task_context<void>(stdexec::inline_scheduler{}));

    // Return current cached value (will be updated when response is received)
    return PowerCapIntf::powerCapEnable();
}

} // namespace platform_mc
} // namespace pldm
