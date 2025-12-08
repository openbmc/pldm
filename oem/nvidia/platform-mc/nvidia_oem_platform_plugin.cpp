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
#include "nvidia_oem_platform_plugin.hpp"

#include "common/utils.hpp"
#include "platform-mc/numeric_effecter.hpp"
#include "platform-mc/numeric_effecter_power_cap_handler.hpp"

#include <phosphor-logging/lg2.hpp>

namespace pldm
{
namespace platform_mc
{
namespace nvidia
{

PHOSPHOR_LOG2_USING;

// Helper to extract data value from PDR
static inline double getEffecterDataValue(uint8_t effecter_data_size,
                                          union_effecter_data_size value)
{
    double ret = std::numeric_limits<double>::quiet_NaN();
    switch (effecter_data_size)
    {
        case PLDM_EFFECTER_DATA_SIZE_UINT8:
            ret = value.value_u8;
            break;
        case PLDM_EFFECTER_DATA_SIZE_SINT8:
            ret = value.value_s8;
            break;
        case PLDM_EFFECTER_DATA_SIZE_UINT16:
            ret = value.value_u16;
            break;
        case PLDM_EFFECTER_DATA_SIZE_SINT16:
            ret = value.value_s16;
            break;
        case PLDM_EFFECTER_DATA_SIZE_UINT32:
            ret = value.value_u32;
            break;
        case PLDM_EFFECTER_DATA_SIZE_SINT32:
            ret = value.value_s32;
            break;
    }
    return ret;
}

void NvidiaOemPlatformPlugin::onEffecterCreated(
    std::weak_ptr<NumericEffecter> weakEffecter,
    const std::shared_ptr<pldm_numeric_effecter_value_pdr>& pdr)
{
    // Lock the weak_ptr to get a shared_ptr
    auto effecter = weakEffecter.lock();
    if (!effecter || !pdr)
    {
        return;
    }

    // Register power cap handler for WATTS effecters on processor entities
    if (pdr->base_unit == PLDM_SENSOR_UNIT_WATTS &&
        (pdr->entity_type == PLDM_ENTITY_PROC ||
         pdr->entity_type == PLDM_ENTITY_PROC_IO_MODULE))
    {
        try
        {
            auto& bus = pldm::utils::DBusHandler::getBus();

            // Get min/max values in base units (watts)
            double maxValue = effecter->rawToBase(getEffecterDataValue(
                pdr->effecter_data_size, pdr->max_settable));
            double minValue = effecter->rawToBase(getEffecterDataValue(
                pdr->effecter_data_size, pdr->min_settable));

            // Create and register power cap handler
            auto powerCapHandler =
                std::make_unique<NumericEffecterPowerCapHandler>(
                    *effecter, bus, effecter->path, minValue, maxValue);

            effecter->registerHandler(std::move(powerCapHandler));

            lg2::info(
                "NVIDIA OEM: Registered power cap handler for effecter {NAME}",
                "NAME", effecter->effecterName);
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "NVIDIA OEM: Failed to register power cap handler for {NAME}: {ERROR}",
                "NAME", effecter->effecterName, "ERROR", e.what());
        }
    }
}

} // namespace nvidia
} // namespace platform_mc
} // namespace pldm
