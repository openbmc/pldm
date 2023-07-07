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

#include <sdbusplus/asio/object_server.hpp>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/asio/steady_timer.hpp>

#include <memory>
#include <string>
#include <functional>

#include "RdeUtils.hpp"

class PwmSensor
{
  public:
    PwmSensor(const std::string& name,
              const std::string& rdeSensorUri,
              const std::string& rdeDevObjPath,
              const std::string& rdeDevService,
              const std::string& rdeDeviceId,
              boost::asio::io_context& io,
              std::shared_ptr<sdbusplus::asio::connection>& conn,
              sdbusplus::bus_t& dbusConn,
              sdbusplus::asio::object_server& objectServer,
              bool isValueMutable = false);
    ~PwmSensor();

    std::string& getRdeDeviceObjPath();
    std::function<void(const boost::system::error_code&)> sendFanPWMFunc;

  private:
    boost::asio::io_context& io;
    std::shared_ptr<sdbusplus::asio::connection>& conn;
    sdbusplus::bus_t& dbusConn;
    sdbusplus::asio::object_server& objectServer;
    boost::asio::steady_timer timer;
    std::string name;
    std::string rdeSensorUri;
    std::string rdeDevObjPath;
    std::string rdeDevService;
    std::string rdeDeviceId;
    std::shared_ptr<sdbusplus::asio::dbus_interface> sensorInterface;
    std::shared_ptr<sdbusplus::asio::dbus_interface> controlInterface;
    std::shared_ptr<sdbusplus::asio::dbus_interface> valueMutabilityInterface;
    uint64_t pwmValue;
};
