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
#include "RdeTachSensor.hpp"
#include "RdeUtils.hpp"

#include <unistd.h>

#include <boost/asio/read_until.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <charconv>
#include <fstream>
#include <iostream>
#include <istream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

TachSensor::TachSensor(const std::string& name,
                       const std::string& rdeDevObjPath,
                       std::shared_ptr<sdbusplus::asio::connection>& conn,
                       sdbusplus::asio::object_server& objectServer,
                       bool isValueMutable) :
    objectServer(objectServer),
    name(name), rdeDevObjPath(rdeDevObjPath)
{
    sensorInterface = objectServer.add_interface(
        "/xyz/openbmc_project/sensors/fan_tach/" + name,
        "xyz.openbmc_project.Sensor.Value");

    sensorInterface->register_property("Value", 0);
    sensorInterface->register_property("MaxValue", 25000);
    sensorInterface->register_property("MinValue", 0);
    sensorInterface->register_property(
        "Unit", std::string("xyz.openbmc_project.Sensor.Value.Unit.RPMS"));

    sensorInterface->initialize();

    if (isValueMutable)
    {
        valueMutabilityInterface =
            std::make_shared<sdbusplus::asio::dbus_interface>(
                conn, sensorInterface->get_object_path(),
                valueMutabilityInterfaceName);
        valueMutabilityInterface->register_property("Mutable", true);
        if (!valueMutabilityInterface->initialize())
        {
            std::cerr
                << "error initializing sensor value mutability interface\n";
            valueMutabilityInterface = nullptr;
        }
    }

    // TODO Nikhil : Check if we need to associate rde pwm/tach sensors with
    // chassis ?
}

TachSensor::~TachSensor()
{
    objectServer.remove_interface(sensorInterface);
    objectServer.remove_interface(valueMutabilityInterface);
}

/**
 * @brief Get the Rde device object path for the tach sensor
 *
 * @return std::string (Rde device object path)
 */

std::string& TachSensor::getRdeDeviceObjPath()
{
    return rdeDevObjPath;
}
