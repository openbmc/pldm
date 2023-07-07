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
#include "RdeUtils.hpp"

namespace fs = std::filesystem;


/**
 * @brief Generate a random unique integer
 *
 * @return int (Random unique integer)
 */

static inline int generateRandomInt()
{
    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    std::uniform_int_distribution<> distribution(1, 9999);
    int randomNumber = distribution(generator);
    return randomNumber;
}

bool isResponseError(const std::string& resp)
{
    if (resp == "ERROR")
    {
        return true;
    }
    return false;
}

void createAssociation(
    std::shared_ptr<sdbusplus::asio::dbus_interface>& association,
    const std::string& path)
{
    std::cerr << "Inside createAssociation" << std::endl;
    if (association)
    {
        fs::path p(path);

        std::vector<Association> associations;
        associations.emplace_back("chassis", "all_sensors",
                                  p.parent_path().string());
        association->register_property("Associations", associations);
        association->initialize();
        std::cerr << "Done createAssociation" << std::endl;
    }
}

std::vector<std::unique_ptr<sdbusplus::bus::match_t>>
    setupRdeDeviceDetectionMatches(
        sdbusplus::bus_t& bus,
        const std::function<void(sdbusplus::message_t&)>& handler)
{
    std::vector<std::unique_ptr<sdbusplus::bus::match_t>> matches;
    auto match = std::make_unique<sdbusplus::bus::match_t>(
        bus,
        sdbusplus::bus::match::rules::propertiesChangedNamespace(
            rdeDevicesPath, rdeDeviceInterface),
        handler);
    matches.emplace_back(std::move(match));

    return matches;
}

std::unique_ptr<sdbusplus::bus::match_t>
    setupRdeDeviceRemovalMatches(
        sdbusplus::bus_t& bus,
        const std::function<void(sdbusplus::message_t&)>& handler)
{
    std::unique_ptr<sdbusplus::bus::match_t> match =
        std::make_unique<sdbusplus::bus::match_t>(
            bus,
            sdbusplus::bus::match::rules::interfacesRemoved() +
                sdbusplus::bus::match::rules::argNpath(
                    0, std::string(rdeDevicesPath) + "/"),
            handler);
    return match;
}

std::string sendRdeDbusRequest(sdbusplus::bus_t& dbusConnection,
                               const std::string& uri, const std::string& rdeDeviceId,
                               const std::string& rdeServiceName, const std::string& rdeObjPath,
                               pldm_rde_operation_type operationType,
                               const std::string& requestPayload)
{
    std::string resp;
    sdbusplus::message_t rdeRequest =
    dbusConnection.new_method_call(
        rdeServiceName.c_str(), rdeObjPath.c_str(),
        rdeDeviceInterface,  "execute_rde");
    rdeRequest.append(generateRandomInt(), (uint8_t)operationType,
        uri.c_str(), rdeDeviceId.c_str(), requestPayload.c_str());
    try
    {
        sdbusplus::message_t reply =
            dbusConnection.call(rdeRequest);
        reply.read(resp);
    }
    catch (const sdbusplus::exception_t& e)
    {
        std::cerr << "While calling execute_rde on service:"
                    << rdeServiceName << " exception name:" << e.name()
                    << "and description:" << e.description()
                    << " was thrown\n";
        return "ERROR";
    }
    return resp;
}
