/**
 * Copyright 2023 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once
#include "libpldm/pldm_rde.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/container/flat_map.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/message/types.hpp>

#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <regex>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

const constexpr char* rdeDevicesPath = "/xyz/openbmc_project/rde_devices";
const constexpr char* unitPercent =
    "xyz.openbmc_project.Sensor.Value.Unit.Percent";
const constexpr char* valueMutabilityInterfaceName =
    "xyz.openbmc_project.Sensor.ValueMutability";
const constexpr char* rdeDeviceInterface = "xyz.openbmc_project.RdeDevice";

using ObjPathToServiceMapType = std::map<std::string, std::string>;
using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;
using Association = std::tuple<std::string, std::string, std::string>;

namespace mapper
{
constexpr const char* busName = "xyz.openbmc_project.ObjectMapper";
constexpr const char* path = "/xyz/openbmc_project/object_mapper";
constexpr const char* interface = "xyz.openbmc_project.ObjectMapper";
constexpr const char* subtree = "GetSubTree";
} // namespace mapper

namespace association
{
const static constexpr char* interface =
    "xyz.openbmc_project.Association.Definitions";
} // namespace association

/**
 * @brief Check if it is an error response
 *
 * @param[in] resp A json object storing the response
 *
 * @return bool (True for error response and false otherwise)
 */

bool isResponseError(const std::string& resp);

/**
 * @brief Create association between D-bus objects
 *
 * @param[in] association Pointer to an associations interface
 * @param[in] path A D-bus object path
 *
 * @return void
 */

void createAssociation(
    std::shared_ptr<sdbusplus::asio::dbus_interface>& association,
    const std::string& path);

/**
 * @brief Setup a matcher to detect Rde device D-bus object discovery
 *
 * @param[in] bus A D-bus bus object
 * @param[in] handler A callback function to be called when match is found
 *
 * @return std::vector<std::unique_ptr<sdbusplus::bus::match_t>> (Match object)
 */

std::vector<std::unique_ptr<sdbusplus::bus::match_t>>
    setupRdeDeviceDetectionMatches(
        sdbusplus::bus_t& bus,
        const std::function<void(sdbusplus::message_t&)>& handler);

/**
 * @brief Setup a matcher to detect Rde device D-bus object removal
 *
 * @param[in] bus A D-bus bus object
 * @param[in] handler A callback function to be called when match is found
 *
 * @return std::vector<std::unique_ptr<sdbusplus::bus::match_t>> (Match object)
 */

std::unique_ptr<sdbusplus::bus::match_t> setupRdeDeviceRemovalMatches(
    sdbusplus::bus_t& bus,
    const std::function<void(sdbusplus::message_t&)>& handler);

/**
 * @brief Send Redfish queries to Rde device
 *
 * @param[in] dbusConnection A D-bus bus object to send D-bus requests
 * @param[in] uri A Redfish resource uri to query
 * @param[in] rdeDeviceId Rde device id
 * @param[in] rdeServiceName Name of the service that handles D-bus requests for
 * Rde device
 * @param[in] rdeObjPath Rde device D-bus object path
 * @param[in] operationType Pldm operation type
 * @param[in] requestPayload Request payload to send
 *
 * @return std::string (Response for the Redfish query)
 */

std::string sendRdeDbusRequest(sdbusplus::bus_t& dbusConnection,
                               const std::string& uri,
                               const std::string& rdeDeviceId,
                               const std::string& rdeServiceName,
                               const std::string& rdeObjPath,
                               pldm_rde_operation_type operationType,
                               const std::string& requestPayload);

/** @struct GetRdeDevices
 *
 *  Container that stores data and method needed to get the Rde device object
 * paths
 */
struct GetRdeDevices : std::enable_shared_from_this<GetRdeDevices>
{
    GetRdeDevices(std::shared_ptr<sdbusplus::asio::connection> connection,
                  std::function<void(ObjPathToServiceMapType& serviceMap)>&&
                      callbackFunc) :
        dbusConnection(std::move(connection)),
        callback(std::move(callbackFunc))
    {}

    void getRdeDevices(size_t retries = 0)
    {
        if (retries > 5)
        {
            retries = 5;
        }

        std::cerr << "Inside getRdeDevices " << std::endl;
        std::vector<std::string> interfaces = {rdeDeviceInterface};
        std::shared_ptr<GetRdeDevices> self = shared_from_this();
        dbusConnection->async_method_call(
            [self, retries](const boost::system::error_code& ec,
                            const GetSubTreeType& subtree) {
                if (ec)
                {
                    std::cerr << "Error in the mapper call. ec : " << ec
                              << std::endl;
                    if (retries == 0U)
                    {
                        return;
                    }
                    auto timer = std::make_shared<boost::asio::steady_timer>(
                        self->dbusConnection->get_io_context());
                    timer->expires_after(std::chrono::seconds(10));
                    timer->async_wait([self, timer, retries](
                                          const boost::system::error_code& ec) {
                        std::cerr << "Inside timer callback" << std::endl;
                        if (ec)
                        {
                            std::cerr << "Timer error" << std::endl;
                            return;
                        }
                        self->getRdeDevices(retries - 1);
                    });

                    return;
                }

                std::cerr << "Out of dbus error" << std::endl;
                for (const auto& [path, objDict] : subtree)
                {
                    if (objDict.empty())
                    {
                        return;
                    }

                    const std::string& owner = objDict.begin()->first;
                    std::cerr << " Got path : " << path << " with owner "
                              << owner << std::endl;
                    self->rdeDevObjPathsToServiceMap[path] =
                        objDict.begin()->first;
                }
            },
            mapper::busName, mapper::path, mapper::interface, mapper::subtree,
            "/", 0, interfaces);
    }

    ~GetRdeDevices()
    {
        callback(rdeDevObjPathsToServiceMap);
    }

    std::shared_ptr<sdbusplus::asio::connection> dbusConnection;
    std::function<void(ObjPathToServiceMapType& serviceMap)> callback;
    ObjPathToServiceMapType rdeDevObjPathsToServiceMap;
};
