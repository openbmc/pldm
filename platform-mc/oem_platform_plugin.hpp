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

#include <memory>

namespace pldm
{
namespace platform_mc
{

class NumericEffecter;

/**
 * @brief OemPlatformPlugin
 *
 * Interface for OEM-specific platform plugins. OEMs can implement this
 * interface to inject custom handlers, perform OEM-specific initialization,
 * and customize platform behavior at key lifecycle points.
 *
 * This allows OEM-specific code to be cleanly separated from the core PLDM
 * implementation while still having access to important lifecycle hooks.
 */
class OemPlatformPlugin
{
  public:
    virtual ~OemPlatformPlugin() = default;

    /**
     * @brief Hook called after a NumericEffecter is created
     *
     * This is called after the effecter object is created and basic
     * initialization is complete, but before it's added to the terminus.
     * OEMs can use this to:
     * - Register custom D-Bus handlers
     * - Perform OEM-specific initialization
     * - Add custom interfaces
     * - Set up OEM-specific monitoring
     *
     * Note: Effecter is passed as weak_ptr to avoid extending its lifetime.
     * Plugins observe effecters but don't own them. Always check with .lock()
     * before use.
     *
     * @param[in] effecter - Weak pointer to the newly created effecter
     * @param[in] pdr - The PDR used to create the effecter
     */
    virtual void onEffecterCreated(
        [[maybe_unused]] std::weak_ptr<NumericEffecter> effecter,
        [[maybe_unused]] const std::shared_ptr<pldm_numeric_effecter_value_pdr>&
            pdr)
    {}
};

} // namespace platform_mc
} // namespace pldm
