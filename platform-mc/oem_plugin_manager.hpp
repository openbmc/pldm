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

#include "oem_platform_plugin.hpp"

#include <libpldm/platform.h>

#include <memory>
#include <vector>

namespace pldm
{
namespace platform_mc
{

class NumericEffecter;

/**
 * @brief PluginManager
 *
 * Manages registration and invocation of OEM platform plugins.
 * This class handles the lifecycle of OEM plugins, allowing multiple
 * vendors to register their plugins and receive notifications about
 * platform events.
 */
class OemPluginManager
{
  public:
    OemPluginManager() = default;
    ~OemPluginManager() = default;

    OemPluginManager(const OemPluginManager&) = delete;
    OemPluginManager& operator=(const OemPluginManager&) = delete;
    OemPluginManager(OemPluginManager&&) = default;
    OemPluginManager& operator=(OemPluginManager&&) = default;

    /**
     * @brief Register an OEM platform plugin
     *
     * Multiple plugins can be registered. Each will be notified of
     * lifecycle events in the order they were registered.
     * PluginManager takes ownership of the plugin.
     *
     * @param[in] plugin - Unique pointer to OEM plugin (ownership transferred)
     */
    void registerPlugin(std::unique_ptr<OemPlatformPlugin> plugin);

    /**
     * @brief Invoke all registered plugins for effecter creation
     *
     * Effecter is passed as weak_ptr to plugins to avoid extending its
     * lifetime. Plugins observe effecters but don't own them.
     *
     * @param[in] effecter - Shared pointer to the newly created effecter
     *                       (converted to weak_ptr when passed to plugins)
     * @param[in] pdr - The PDR used to create the effecter
     */
    void invokeEffecterCreated(
        std::shared_ptr<NumericEffecter> effecter,
        const std::shared_ptr<pldm_numeric_effecter_value_pdr>& pdr);

    /**
     * @brief Get the number of registered plugins
     *
     * @return size_t - Number of registered plugins
     */
    size_t getPluginCount() const
    {
        return plugins.size();
    }

  private:
    /** @brief List of registered OEM platform plugins */
    std::vector<std::unique_ptr<OemPlatformPlugin>> plugins;
};

} // namespace platform_mc
} // namespace pldm
