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
#include <boost/algorithm/string/replace.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio/steady_timer.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus/match.hpp>
#include <boost/url/url.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include <map>
#include <cstdio>

#include "nlohmann/json.hpp"
#include "RdeUtils.hpp"
#include "RdePwmSensor.hpp"
#include "RdeTachSensor.hpp"

/**
 * @brief Determine the tray id of the tray associated with a Rde device
 *
 * @param[in]     trayType  The type of the tray
 * @param[in,out] trayCountMap  A map that stores tray count against tray type
 * @param[in]     rdeDeviceId  Rde device id
 * @param[in,out] rdeIdToTrayIdMap A map for storing rde device id against tray id
 *
 * @return std::string (Tray id)
 */

static std::string getTrayId(const std::string& trayType,
                             boost::container::flat_map<std::string, int> &trayCountMap,
                             const std::string &rdeDeviceId,
                             boost::container::flat_map<std::string, std::string> &rdeIdToTrayIdMap)
{
    std::string trayId = "";
    int count = 0;
    if (rdeIdToTrayIdMap.find(rdeDeviceId) == rdeIdToTrayIdMap.end())
    {
        if (trayCountMap.find(trayType) == trayCountMap.end())
        {
            trayCountMap.emplace(trayType, count);
        }
        else
        {
            count = trayCountMap[trayType];
            trayCountMap[trayType] = count + 1;
        }
        std::cerr << "Adding entry in trayCountMap "
                  << trayType << ":" << count << std::endl;

        trayId = trayType + std::to_string(count);
        // Once a trayId is mapped with rdeDeviceId, the mapping will stay forever
        // even if the rde device goes down
        rdeIdToTrayIdMap.emplace(rdeDeviceId, trayId);
    }
    else
    {
        trayId = rdeIdToTrayIdMap[rdeDeviceId];
    }
    return trayId;
}

/**
 * @brief Determine the Rde pwm sensors on the Rde device
 *
 * @param[in]     dbusConnection  A D-bus bus object to send D-bus requests
 * @param[in]     sensorCollectionRespJson  A json object storing the sensors collection response
                  from Rde device
 * @param[in]     trayId  The tray id
 * @param[in]     rdeDeviceId Rde device id
 * @param[in]     rdeDevService Name of the service that handles D-bus requests for Rde device
 * @param[in]     rdeDevObjPath Rde device D-bus object path
 *
 * @return std::map<std::string, std::string> (Map that stores sensor Uri against unique sensor id)
 */

static std::map<std::string, std::string> findRdePwmSensors(sdbusplus::bus_t &dbusConnection,
                                                            const nlohmann::json &sensorCollectionRespJson,
                                                            const std::string &trayId,
                                                            const std::string &rdeDeviceId, std::string &rdeDevService,
                                                            const std::string &rdeDevObjPath)
{

    std::map<std::string, std::string> rdePwmSensors;

    int sensorsCount = sensorCollectionRespJson["Members@odata.count"];

    std::vector<nlohmann::json> members = sensorCollectionRespJson["Members"];
    for (int i = 0; i < sensorsCount; i++)
    {
        nlohmann::json member = members[i];
        std::cerr << "Getting member ID" << std::endl;
        std::string sensorUri = member["@odata.id"];

        std::cerr << "Sending request for " + sensorUri << std::endl;
        std::string sensorResp = sendRdeDbusRequest(dbusConnection, sensorUri, rdeDeviceId,
                                                    rdeDevService, rdeDevObjPath,
                                                    PLDM_RDE_OPERATION_READ, std::string(""));
        if (isResponseError(sensorResp))
        {
            std::cerr << "Unable to get sensor response" << std::endl;
            continue;
        }

        nlohmann::json sensorRespJson;
        try
        {
            sensorRespJson = nlohmann::json::parse(sensorResp);
        }
        catch (const nlohmann::json::parse_error &e)
        {
            std::cerr << "JSON parsing error: " << e.what()
                      << " while parsing sensor response " << sensorResp << std::endl;
            continue;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error: " << e.what() << " while parsing sensor response " << sensorResp << std::endl;
            continue;
        }

        std::string sensorId = sensorRespJson["Id"];

        std::cerr << "Found sensor ID : " << sensorId << std::endl;
        if (regex_match(sensorId, std::regex(R"(host_fan(\d+)_pwm)")))
        {
            rdePwmSensors[sensorId + "_" + trayId] = sensorUri;
            std::cerr << "Found regex match" << std::endl;
            std::cerr << "findRdePwmSensors) Found : " << sensorId + "_" + trayId << std::endl;
        }
    }
    return rdePwmSensors;
}


/**
 * @brief Create fan pwm and tach sensors for each pwm sensor present on the Rde devices
 *
 * @param[in,out] trayCountMap  A map that stores tray count against tray type
 * @param[in,out] rdeIdToTrayIdMap A map for storing rde device id against tray id
 * @param[in]     io  An asio io context
 * @param[in]     objectServer An asio object server
 * @param[in,out] pwmSensors A map that stores pwm sensor objects against pwm sensor names
 * @param[in,out] tachSensors A map that stores tach sensor objects against tach sensor names
 * @param[in]     asioConnection An asio connection
 * @param[in]     dbusConnection  A D-bus object to send D-bus requests
 * @param[in,out] rdeDevicesChanged A shared pointer to a set that stores the Rde object paths for the
 *                Rde devices that were newly added or removed
 * @param[in]     retries The retry count for running D-bus query to get the Rde device
 *
 * @return void
 */

void createSensors(
    boost::container::flat_map<std::string, int> &trayCountMap,
    boost::container::flat_map<std::string, std::string> &rdeIdToTrayIdMap,
    boost::asio::io_context &io, sdbusplus::asio::object_server &objectServer,
    boost::container::flat_map<std::string, std::unique_ptr<PwmSensor>> &
        pwmSensors,
    boost::container::flat_map<std::string, std::unique_ptr<TachSensor>> &
        tachSensors,
    std::shared_ptr<sdbusplus::asio::connection> &asioConnection,
    sdbusplus::bus_t &dbusConnection,
    const std::shared_ptr<boost::container::flat_set<std::string>> &rdeDevicesChanged,
    size_t retries = 0)
{
    auto getter = std::make_shared<GetRdeDevices>(
        asioConnection,
        [&trayCountMap, &rdeIdToTrayIdMap, &io, &objectServer, &pwmSensors, &tachSensors, &asioConnection,
         &dbusConnection, rdeDevicesChanged](const std::map<std::string, std::string> rdeDevObjPathsToServiceMap)
        {
            bool firstScan = (rdeDevicesChanged == nullptr);
            for (auto it1 = rdeDevObjPathsToServiceMap.begin(); it1 != rdeDevObjPathsToServiceMap.end(); ++it1)
            {

                std::string rdeDevObjPath = it1->first;
                std::string rdeDevService = it1->second;
                sdbusplus::message::object_path path(rdeDevObjPath);
                std::string rdeDeviceId = path.filename();

                // On re-scans only update sensors for newly discovered Rde devices
                if (!firstScan && (rdeDevicesChanged->find(rdeDevObjPath) == rdeDevicesChanged->end()))
                {
                    continue;
                }

                std::cerr << "rdeDeviceId : " << rdeDeviceId << " was changed/updated" << std::endl;

                std::string trayResp = sendRdeDbusRequest(dbusConnection, "/redfish/v1/Chassis/Tray",
                                                          rdeDeviceId, rdeDevService, rdeDevObjPath,
                                                          PLDM_RDE_OPERATION_READ, std::string(""));

                if (isResponseError(trayResp))
                {
                    std::cerr << "Unable to get tray collection" << std::endl;
                    continue;
                }

                nlohmann::json trayRespJson;
                try
                {
                    trayRespJson = nlohmann::json::parse(trayResp);
                }
                catch (const nlohmann::json::parse_error &e)
                {
                    std::cerr << "JSON parsing error: " << e.what()
                              << " while parsing tray response " << trayResp << std::endl;
                    continue;
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error: " << e.what() << " while parsing tray response " << trayResp << std::endl;
                    continue;
                }
                std::string trayId = getTrayId(trayRespJson["Model"], trayCountMap,
                                               rdeDeviceId, rdeIdToTrayIdMap);

                // Get Sensor Collection
                std::string sensorCollectionResp = sendRdeDbusRequest(dbusConnection, "/redfish/v1/Chassis/Tray/Sensors",
                                                                      rdeDeviceId, rdeDevService, rdeDevObjPath,
                                                                      PLDM_RDE_OPERATION_READ, std::string(""));
                if (isResponseError(sensorCollectionResp))
                {
                    std::cerr << "Unable to get sensor collection" << std::endl;
                    return;
                }
                nlohmann::json sensorCollectionRespJson;
                try
                {
                    sensorCollectionRespJson = nlohmann::json::parse(sensorCollectionResp);
                }
                catch (const nlohmann::json::parse_error &e)
                {
                    std::cerr << "JSON parsing error: " << e.what()
                              << " while parsing sensor collection response " << sensorCollectionResp << std::endl;
                    continue;
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error: " << e.what() << " while parsing sensor collection response " << sensorCollectionResp << std::endl;
                    continue;
                }

                std::map<std::string, std::string> rdePwmSensors = findRdePwmSensors(dbusConnection, sensorCollectionRespJson, trayId,
                                                                                     rdeDeviceId, rdeDevService, rdeDevObjPath);
                for (auto it2 = rdePwmSensors.begin(); it2 != rdePwmSensors.end(); ++it2)
                {
                    std::string fanPwmName = it2->first;
                    std::cerr << "Update sensor signal request received for : " << fanPwmName << std::endl;

                    if ((pwmSensors.count(fanPwmName) == 0))
                    {
                        std::cerr << "Rde sensor found in config : " << fanPwmName << std::endl;
                        pwmSensors[fanPwmName] = std::make_unique<PwmSensor>(
                            fanPwmName, rdePwmSensors[fanPwmName], rdeDevObjPath, rdeDevService,
                            rdeDeviceId, io, asioConnection, dbusConnection, objectServer,
                            true);

                        // Create dummy tach sensors with default 0 RPM (one for each pwm sensor)
                        // to make phosphor-pid-control to skip fan loop (b/278600144)
                        std::string fanTachName = fanPwmName;
                        int position = fanTachName.find("pwm");
                        fanTachName.replace(position, 3, "tach");
                        tachSensors[fanTachName] = std::make_unique<TachSensor>(
                            fanTachName, rdeDevObjPath, asioConnection, objectServer,
                            true);
                    }
                }
            }
            // Clear rdeDevicesChanged container since all the changed rde devices have been processed
            if (!firstScan)
            {
                rdeDevicesChanged->clear();
            }
        });
    getter->getRdeDevices(retries);
}

int main()
{

    // TODO Nikhil : Remove this later once b/283151098 is addressed
    // We are allowing rded daemon to be ready after initializing miniBMCs
    sleep(180);
    boost::asio::io_context io;

    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objectServer(systemBus, true);
    sdbusplus::bus_t& dbusConnection = static_cast<sdbusplus::bus_t&>(*systemBus);

    objectServer.add_manager("/xyz/openbmc_project/sensors");
    objectServer.add_manager("/xyz/openbmc_project/control");
    objectServer.add_manager("/xyz/openbmc_project/inventory");
    systemBus->request_name("xyz.openbmc_project.RdeFanSensor");

    boost::container::flat_map<std::string, int> trayCountMap;
    // Once a trayId is mapped with rdeDeviceId, the mapping will stay forever
    // even if the rde device goes down. This is because rdeDeviceId for a given RDE
    // device will stay the same at runtime.
    boost::container::flat_map<std::string, std::string> rdeIdToTrayIdMap;
    boost::container::flat_map<std::string, std::unique_ptr<PwmSensor>>
        pwmSensors;
    boost::container::flat_map<std::string, std::unique_ptr<TachSensor>>
        tachSensors;
    auto rdeDevicesChanged =
        std::make_shared<boost::container::flat_set<std::string>>();

    boost::asio::post(io, [&]()
                      { createSensors(trayCountMap, rdeIdToTrayIdMap,
                                      io, objectServer, pwmSensors, tachSensors, systemBus, dbusConnection, nullptr); });

    boost::asio::steady_timer filterTimer(io);
    std::function<void(sdbusplus::message_t &)> rdeDeviceDetectionHandler =
        [&](sdbusplus::message_t &message)
    {
        if (message.is_method_error())
        {
            std::cerr << "callback method error\n";
            return;
        }
        std::cerr << "Inserting " << message.get_path() << " in rdeDevicesChanged" << std::endl;
        rdeDevicesChanged->insert(message.get_path());
        // this implicitly cancels the timer
        filterTimer.expires_after(std::chrono::seconds(1));

        filterTimer.async_wait([&](const boost::system::error_code &ec)
                               {
            if (ec == boost::asio::error::operation_aborted)
            {
                /* we were canceled*/
                return;
            }
            if (ec)
            {
                std::cerr << "timer error\n";
                return;
            }
            createSensors(trayCountMap, rdeIdToTrayIdMap, io, objectServer, pwmSensors, tachSensors,
                          systemBus, dbusConnection, rdeDevicesChanged, 5); });
    };

    std::function<void(sdbusplus::message_t &)> rdeDeviceRemovalHandler =
        [&](sdbusplus::message_t &message)
    {
        if (message.is_method_error())
        {
            std::cerr << "callback method error\n";
            return;
        }
        std::cerr << "Removing " << message.get_path() << " from pwmSensors" << std::endl;
        sdbusplus::message::object_path changedObject;
        std::vector<std::string> interfacesRemoved;
        message.read(changedObject, interfacesRemoved);
        std::string removedObjectPath = std::string(changedObject);

        auto intfIt = std::find(interfacesRemoved.begin(), interfacesRemoved.end(), rdeDeviceInterface);
        if (intfIt != interfacesRemoved.end())
        {
            std::cerr << "Need to remove " << removedObjectPath << " since RDE device got removed" << std::endl;
            // When RDE device is removed, delete the corresponding pwm and tach sensors
            // created for the device
            auto it1 = pwmSensors.begin();
            while (it1 != pwmSensors.end())
            {
                std::string& pwmObjPath = it1->second->getRdeDeviceObjPath();
                std::cerr << "pwmObjPath : " << pwmObjPath << std::endl;
                if (pwmObjPath == removedObjectPath)
                {
                    std::cerr << "Removed " << pwmObjPath << std::endl;
                    it1 = pwmSensors.erase(it1);
                    break;
                }
                it1++;
            }

            auto it2 = tachSensors.begin();
            while (it2 != tachSensors.end())
            {
                std::string& tachObjPath = it2->second->getRdeDeviceObjPath();
                std::cerr << "tachObjPath : " << tachObjPath << std::endl;
                if (tachObjPath == removedObjectPath)
                {
                    std::cerr << "Removed " << tachObjPath << std::endl;
                    it2 = tachSensors.erase(it2);
                    break;
                }
                it2++;
            }
        }
    };

    std::vector<std::unique_ptr<sdbusplus::bus::match_t>> rdeDeviceDetectionMatches =
        setupRdeDeviceDetectionMatches(dbusConnection, rdeDeviceDetectionHandler);

    std::unique_ptr<sdbusplus::bus::match_t> rdeDeviceRemovalMatche =
        setupRdeDeviceRemovalMatches(dbusConnection, rdeDeviceRemovalHandler);

    io.run();
    return 0;
}
