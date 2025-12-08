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

#include <libpldm/platform.h>

#include <cstdint>

namespace pldm
{
namespace platform_mc
{

class NumericEffecter;

/**
 * @brief NumericEffecterDbusHandler
 *
 * Interface for handlers that respond to numeric effecter value changes.
 * Implementations of this interface can be registered with a NumericEffecter
 * to receive notifications when the effecter's value or state changes.
 */
class NumericEffecterDbusHandler
{
  public:
    virtual ~NumericEffecterDbusHandler() = default;

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
