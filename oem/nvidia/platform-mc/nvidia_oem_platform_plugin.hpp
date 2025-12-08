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

#include "platform-mc/oem_platform_plugin.hpp"

namespace pldm
{
namespace platform_mc
{
namespace nvidia
{

/**
 * @brief NvidiaOemPlatformPlugin
 *
 * NVIDIA-specific implementation of OEM platform plugin.
 * Handles NVIDIA-specific effecter and sensor configurations, including:
 * - Power cap effecters for processor power management
 * - Custom D-Bus handlers for NVIDIA-specific interfaces
 * - OEM-specific initialization and monitoring
 */
class NvidiaOemPlatformPlugin : public OemPlatformPlugin
{
  public:
    NvidiaOemPlatformPlugin() = default;
    ~NvidiaOemPlatformPlugin() override = default;

    /**
     * @brief Handle NVIDIA-specific effecter creation
     *
     * Registers NVIDIA-specific handlers based on effecter type:
     * - Power cap handler for WATTS effecters on PROC entities
     * - Future: Other NVIDIA-specific handlers
     */
    void onEffecterCreated(
        std::weak_ptr<NumericEffecter> effecter,
        const std::shared_ptr<pldm_numeric_effecter_value_pdr>& pdr) override;
};

} // namespace nvidia
} // namespace platform_mc
} // namespace pldm
