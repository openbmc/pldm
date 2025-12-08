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
#pragma once

#include "platform-mc/numeric_effecter_dbus_handler.hpp"

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
class NumericEffecterPowerCapHandler :
    public NumericEffecterDbusHandler,
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
    NumericEffecterPowerCapHandler(
        NumericEffecter& effecter, sdbusplus::bus_t& bus,
        const std::string& path, double minValue, double maxValue);

    ~NumericEffecterPowerCapHandler() override = default;

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
