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
#include "oem_plugin_manager.hpp"

#include "numeric_effecter.hpp"

#include <phosphor-logging/lg2.hpp>

namespace pldm
{
namespace platform_mc
{

PHOSPHOR_LOG2_USING;

void OemPluginManager::registerPlugin(std::unique_ptr<OemPlatformPlugin> plugin)
{
    if (!plugin)
    {
        lg2::warning("Attempted to register null OEM platform plugin");
        return;
    }

    plugins.push_back(std::move(plugin));
    lg2::info("Registered OEM platform plugin ({COUNT} total)", "COUNT",
              plugins.size());
}

void OemPluginManager::invokeEffecterCreated(
    std::shared_ptr<NumericEffecter> effecter,
    const std::shared_ptr<pldm_numeric_effecter_value_pdr>& pdr)
{
    if (!effecter)
    {
        lg2::warning("Attempted to invoke plugin with null effecter");
        return;
    }

    // Convert to weak_ptr to pass to plugins
    // Plugins should observe, not own effecters
    std::weak_ptr<NumericEffecter> weakEffecter = effecter;

    for (auto& plugin : plugins)
    {
        if (plugin)
        {
            try
            {
                plugin->onEffecterCreated(weakEffecter, pdr);
            }
            catch (const std::exception& e)
            {
                lg2::error(
                    "Exception in OEM plugin onEffecterCreated for {NAME}: {ERROR}",
                    "NAME", effecter->effecterName, "ERROR", e.what());
            }
        }
    }
}

} // namespace platform_mc
} // namespace pldm
