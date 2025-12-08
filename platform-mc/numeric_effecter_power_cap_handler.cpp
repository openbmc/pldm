/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "platform-mc/numeric_effecter_power_cap_handler.hpp"

#include "platform-mc/numeric_effecter.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/stdexec/task.hpp>

#include <limits>

namespace pldm
{
namespace platform_mc
{

PHOSPHOR_LOG2_USING;

NumericEffecterPowerCapHandler::NumericEffecterPowerCapHandler(
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
            "Created Power Cap handler for effecter {NAME} with min={MIN}W max={MAX}W",
            "NAME", effecter.effecterName, "MIN", minValue, "MAX", maxValue);
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to initialize Power Cap D-Bus interface for {PATH}: {ERROR}",
            "PATH", path, "ERROR", e);
        throw;
    }
}

void NumericEffecterPowerCapHandler::handleValueChange(
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
            value = 0;
            enabled = false;
            break;
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
                "NAME", effecter.effecterName, "VALUE", value, "ENABLED",
                enabled);
        }
        else
        {
            lg2::debug("Disabled power cap for {NAME}", "NAME",
                       effecter.effecterName);
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to update Power Cap D-Bus properties for {NAME}: {ERROR}",
            "NAME", effecter.effecterName, "ERROR", e);
    }
}

void NumericEffecterPowerCapHandler::handleError(
    [[maybe_unused]] NumericEffecter& effecter)
{
    try
    {
        // On error, set to disabled state
        PowerCapIntf::powerCapEnable(false, false);
        PowerCapIntf::powerCap(0, false);
        lg2::debug("Set power cap to disabled state due to error for {NAME}",
                   "NAME", effecter.effecterName);
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to update Power Cap D-Bus properties on error for {NAME}: {ERROR}",
            "NAME", effecter.effecterName, "ERROR", e);
    }
}

uint32_t NumericEffecterPowerCapHandler::powerCap() const
{
    return PowerCapIntf::powerCap();
}

uint32_t NumericEffecterPowerCapHandler::powerCap(uint32_t value)
{
    // Validate against min/max from PDR
    if (value > PowerCapIntf::maxPowerCapValue() ||
        value < PowerCapIntf::minPowerCapValue())
    {
        lg2::error(
            "Power cap value {VALUE}W out of range [{MIN}W, {MAX}W] for {NAME}",
            "VALUE", value, "MIN", PowerCapIntf::minPowerCapValue(), "MAX",
            PowerCapIntf::maxPowerCapValue(), "NAME", effecter.effecterName);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    lg2::info("Setting power cap for {NAME} to {VALUE}W", "NAME",
              effecter.effecterName, "VALUE", value);

    // Convert to raw value and send to terminus
    double baseValue = static_cast<double>(value);
    double rawValue = effecter.baseToRaw(baseValue);

    // Start detached coroutine to send the command asynchronously
    stdexec::start_detached(
        effecter.setNumericEffecterValue(rawValue),
        exec::default_task_context<void>(exec::inline_scheduler{}));

    // Return current cached value (will be updated when response is received)
    return PowerCapIntf::powerCap();
}

bool NumericEffecterPowerCapHandler::powerCapEnable() const
{
    return PowerCapIntf::powerCapEnable();
}

bool NumericEffecterPowerCapHandler::powerCapEnable(bool value)
{
    pldm_effecter_oper_state newState;
    if (value)
    {
        newState = EFFECTER_OPER_STATE_ENABLED_UPDATEPENDING;
        lg2::info("Enabling power cap for {NAME}", "NAME",
                  effecter.effecterName);
    }
    else
    {
        newState = EFFECTER_OPER_STATE_DISABLED;
        lg2::info("Disabling power cap for {NAME}", "NAME",
                  effecter.effecterName);
    }

    // Start detached coroutine to send the command asynchronously
    stdexec::start_detached(
        effecter.setNumericEffecterEnable(newState),
        exec::default_task_context<void>(exec::inline_scheduler{}));

    // Return current cached value (will be updated when response is received)
    return PowerCapIntf::powerCapEnable();
}

} // namespace platform_mc
} // namespace pldm
