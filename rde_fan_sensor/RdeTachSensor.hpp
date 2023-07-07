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
#include <boost/asio/random_access_file.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class TachSensor
{
public:
  TachSensor(const std::string &name,
             const std::string &rdeDevObjPath,
             std::shared_ptr<sdbusplus::asio::connection> &conn,
             sdbusplus::asio::object_server &objectServer,
             bool isValueMutable);
  ~TachSensor();
  std::string& getRdeDeviceObjPath();

private:
  sdbusplus::asio::object_server &objectServer;
  std::string name;
  std::string rdeDevObjPath;
  std::shared_ptr<sdbusplus::asio::dbus_interface> sensorInterface;
  std::shared_ptr<sdbusplus::asio::dbus_interface> valueMutabilityInterface;
};
