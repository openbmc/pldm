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

#include "common/types.hpp"

#include <libpldm/platform.h>

#include <sdbusplus/async/stdexec/task.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Availability/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

namespace pldm
{
namespace platform_mc
{

class TerminusManager;

using SensorUnit = sdbusplus::xyz::openbmc_project::Sensor::server::Value::Unit;
using Associations =
    std::vector<std::tuple<std::string, std::string, std::string>>;
using ValueIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Sensor::server::Value>;
using OperationalStatusIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::State::
                                    Decorator::server::OperationalStatus>;
using AvailabilityIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::State::Decorator::server::Availability>;
using AssociationDefinitionsInft = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

/**
 * @brief NumericEffecter
 *
 * This class handles effecter value setting and exports
 * status to D-Bus interface.
 */
class NumericEffecter
{
  public:
    NumericEffecter(const pldm_tid_t tid, const bool effecterDisabled,
                    std::shared_ptr<pldm_numeric_effecter_value_pdr> pdr,
                    std::string& effecerName, std::string& associationPath,
                    TerminusManager& terminusManager);
    ~NumericEffecter() {};

    /** @brief The function called to set effecter to
     * error status.
     */
    void handleErrGetNumericEffecterValue();

    /** @brief Updating the effecter status to D-Bus interface
     */
    void updateValue(pldm_effecter_oper_state effecterOperState,
                     double pendingValue, double presentValue);

    /** @brief Sending setNumericEffecterEnable command for the effecter
     *
     *  @param[in] state - the effecter state to be set
     */
    exec::task<int> setNumericEffecterEnable(pldm_effecter_oper_state state);

    /** @brief Sending setNumericEffecterValue command for the effecter
     *
     *  @param[in] effecterValue - the effecter value to be set
     */
    exec::task<int> setNumericEffecterValue(double effecterValue);

    /** @brief Sending getNumericEffecterValue command for the effecter
     */
    exec::task<int> getNumericEffecterValue();

    /** @brief Get the ContainerID, EntityType, EntityInstance of the PLDM
     * Entity which the effecter belongs to
     *  @return EntityInfo - Entity ID
     */
    inline auto getEntityInfo()
    {
        return entityInfo;
    }

    /** @brief Updating the association to D-Bus interface
     *  @param[in] inventoryPath - inventory path of the entity
     */
    inline void setInventoryPaths(const std::vector<std::string>& inventoryPath)
    {
        if (associationDefinitionsIntf)
        {
            std::map<std::pair<std::string, std::string>, bool> assocMap;
            Associations assocs{};

            auto associations = associationDefinitionsIntf->associations();
            for (auto& association : associations)
            {
                auto& [forward, reverse, objectPath] = association;
                auto iter = assocMap.find(std::make_pair(forward, reverse));
                if (iter == assocMap.end())
                {
                    for (const auto& path : inventoryPath)
                    {
                        assocs.emplace_back(std::make_tuple(
                            forward.c_str(), reverse.c_str(), path.c_str()));
                    }
                    assocMap[{forward, reverse}] = true;
                }
            }
            associationDefinitionsIntf->associations(assocs);
        }
    }

    /** @brief Getter of value member variable */
    double getValue()
    {
        return value;
    }

    /** @brief getter of baseUnit member variable */
    uint8_t getBaseUnit()
    {
        return baseUnit;
    }

    /** @brief get the association */
    auto getAssociation() const
    {
        return associationDefinitionsIntf->associations();
    }

    /** @brief Set effecter unit and namespace based on baseUnit
     *
     *  @param[in] baseUnit - The PLDM defined baseUnit enum
     */
    void setEffecterUnit(uint8_t baseUnit);

    /** @brief Terminus ID which the effecter belongs to */
    pldm_tid_t tid;

    /** @brief Effecter name */
    std::string effecterName;

    /** @brief Effecter ID */
    uint16_t effecterId;

    /** @brief ContainerID, EntityType, EntityInstance of the PLDM Entity which
     * the effecter belongs to */
    pldm::pdr::EntityInfo entityInfo;

    /** @brief  The PLDM defined effecterDataSize enum */
    uint8_t dataSize;

    /** @brief  The DBus path of effecter */
    std::string path;

    /** @brief flag to update the value once */
    bool needsUpdate;

  private:
    std::unique_ptr<AvailabilityIntf> availabilityIntf = nullptr;
    std::unique_ptr<OperationalStatusIntf> operationalStatusIntf = nullptr;
    std::unique_ptr<AssociationDefinitionsInft> associationDefinitionsIntf =
        nullptr;

    /** @brief The DBus namespace for effecter based on baseUnit */
    std::string effecterNameSpace;

    /** @brief The resolution of effecter in Units */
    double resolution;

    /** @brief A constant value that is added in as part of conversion process
     * of converting a raw effecter reading to Units */
    double offset;

    /** @brief A power-of-10 multiplier for baseUnit */
    int8_t unitModifier;

    /** @brief reference of TerminusManager */
    TerminusManager& terminusManager;

    /** @brief raw value of numeric effecter */
    double value;

    /** @brief baseUnit of numeric effecter */
    uint8_t baseUnit;
};
} // namespace platform_mc
} // namespace pldm
