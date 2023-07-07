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

#include "RdePwmSensor.hpp"

#include <boost/asio/wait_traits.hpp>

#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

static constexpr double pwmMax = 255.0;
static constexpr double targetIfaceMax = pwmMax;
static constexpr int fanPWMTimeInterval = 120;

PwmSensor::PwmSensor(const std::string& name, const std::string& rdeSensorUri,
                     const std::string& rdeDevObjPath,
                     const std::string& rdeDevService,
                     const std::string& rdeDeviceId,
                     boost::asio::io_context& io,
                     std::shared_ptr<sdbusplus::asio::connection>& conn,
                     sdbusplus::bus_t& dbusConn,
                     sdbusplus::asio::object_server& objectServer,
                     bool isValueMutable) :
    io(io),
    conn(conn), dbusConn(dbusConn), objectServer(objectServer),
    timer(io, boost::asio::chrono::seconds(fanPWMTimeInterval)), name(name),
    rdeSensorUri(rdeSensorUri), rdeDevObjPath(rdeDevObjPath),
    rdeDevService(rdeDevService), rdeDeviceId(rdeDeviceId), pwmValue(0)
{
    sensorInterface = objectServer.add_interface(
        "/xyz/openbmc_project/sensors/fan_pwm/" + name,
        "xyz.openbmc_project.Sensor.Value");

    double fValue = 100.0 * (static_cast<double>(pwmValue) / pwmMax);
    sensorInterface->register_property(
        "Value", fValue,
        [this](const double& req, double& resp) {
            if (!std::isfinite(req))
            {
                // Reject attempted change, if to NaN or other non-numeric
                return -1;
            }
            std::cerr << "inside Sensor set" << std::endl;
            if (req > 100.0 || req < 0.0)
            {
                throw std::runtime_error("Value out of range");
                return -1;
            }

            double reqValue = (req / 100.0) * pwmMax;
            double respValue = (resp / 100.0) * pwmMax;
            auto reqInt = static_cast<uint64_t>(std::round(reqValue));
            auto respInt = static_cast<uint64_t>(std::round(respValue));
            // Avoid floating-point equality, compare as integers
            if (reqInt == respInt)
            {
                return 1;
            }

            this->pwmValue = reqInt;
            resp = req;

            controlInterface->signal_property("Target");

            return 1;
        },
        [this](double& curVal) {
            std::cerr << "inside Sensor get" << std::endl;
            double currScaled = (curVal / 100.0) * pwmMax;
            uint64_t currInt = static_cast<uint64_t>(std::round(currScaled));

            uint64_t pwmVal = this->pwmValue;
            // Avoid floating-point equality, compare as integers
            if (currInt != pwmVal)
            {
                std::cerr << "Inside RDE Sensor Get Callback. curVal : "
                          << curVal << " pwmValue : " << pwmVal << std::endl;
                double getScaled =
                    100.0 * (static_cast<double>(pwmVal) / pwmMax);
                curVal = getScaled;
                controlInterface->signal_property("Target");
                sensorInterface->signal_property("Value");
            }
            return curVal;
        });

    // pwm sensor interface unit is in percentage
    sensorInterface->register_property("MaxValue", static_cast<double>(100));
    sensorInterface->register_property("MinValue", static_cast<double>(0));
    sensorInterface->register_property("Unit", unitPercent);

    controlInterface = objectServer.add_interface(
        "/xyz/openbmc_project/control/fanpwm/" + name,
        "xyz.openbmc_project.Control.FanPwm");
    controlInterface->register_property(
        "Target", static_cast<uint64_t>(pwmValue),
        [this](const uint64_t& req, uint64_t& resp) {
            std::cerr << "inside Control set " << req
                      << " Resp Value : " << resp
                      << " pwmValue : " << this->pwmValue << std::endl;

            if (req > static_cast<uint64_t>(targetIfaceMax))
            {
                throw std::runtime_error("Value out of range");
                return -1;
            }
            if (req == resp)
            {
                return 1;
            }
            auto scaledValue = static_cast<double>(req) / targetIfaceMax;
            auto roundValue = std::round(scaledValue * pwmMax);
            uint64_t pwmVal = static_cast<uint64_t>(roundValue);
            this->pwmValue = pwmVal;

            // Trigger RDE patch operation from here
            resp = req;
            double pwmDutyCycle =
                (static_cast<double>(pwmVal) / pwmMax) * 100.0;
            std::string pwmPatchStr =
                "{\"Reading\":" + std::to_string(pwmDutyCycle) + "}";
            std::cerr << "Sending fanPWMDuty : " << pwmDutyCycle
                      << " for sensor : " << this->rdeSensorUri
                      << " on device : " << this->rdeDeviceId << std::endl;
            this->io.post([&, pwmPatchStr{pwmPatchStr}]() {
                std::string resp = sendRdeDbusRequest(
                    this->dbusConn, this->rdeSensorUri, this->rdeDeviceId,
                    this->rdeDevService, this->rdeDevObjPath,
                    PLDM_RDE_OPERATION_UPDATE, pwmPatchStr);
                if (isResponseError(resp))
                {
                    std::cerr << "Received dbus error response while patch pwm"
                              << std::endl;
                }
            });

            sensorInterface->signal_property("Value");

            return 1;
        },
        [this](uint64_t& curVal) {
            std::cerr << "inside Control get" << std::endl;
            auto scaledValue = static_cast<double>(this->pwmValue) / pwmMax;
            auto roundValue = std::round(scaledValue * targetIfaceMax);
            auto value = static_cast<uint64_t>(roundValue);
            if (curVal != value)
            {
                curVal = value;
                controlInterface->signal_property("Target");
                sensorInterface->signal_property("Value");
            }
            return curVal;
        });

    sensorInterface->initialize();
    controlInterface->initialize();

    /*
       By design, phosphor-pid-conrol only dumps fanPWM to dbus if the value
       is different from the previous one to avoid unnecessary dbus calls and so
       accordingly the rde_fan_sensor daemon sends it to the RDE device. However
       if the RDE device does not recieve a value, it may think that BMC<->RDE
       communication is broken. So setup a  timer to send fanPWM value
       periodically. The timer feature can be removed once, b/277832479 is
       addressed in phosphor-pid-control
    */

    sendFanPWMFunc = [&](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted)
        {
            /* we were canceled*/
            std::cerr << "sendFanPWM timer is being cancelled" << std::endl;
            return;
        }
        if (ec)
        {
            std::cerr << "sendFanPWM timer error" << std::endl;
            return;
        }
        double pwmDutyCycle =
            (static_cast<double>(this->pwmValue) / pwmMax) * 100.0;
        std::string pwmPatchStr =
            "{\"Reading\":" + std::to_string(pwmDutyCycle) + "}";
        std::cerr << "Sending fanPWMDuty after 120 seconds in a timed event : "
                  << pwmDutyCycle << " for sensor : " << this->rdeSensorUri
                  << " on device : " << this->rdeDeviceId << std::endl;
        std::string resp = sendRdeDbusRequest(
            this->dbusConn, this->rdeSensorUri, this->rdeDeviceId,
            this->rdeDevService, this->rdeDevObjPath, PLDM_RDE_OPERATION_UPDATE,
            pwmPatchStr);
        if (isResponseError(resp))
        {
            std::cerr << "Received dbus error response while patch pwm"
                      << std::endl;
        }
        this->timer.expires_after(
            boost::asio::chrono::seconds(fanPWMTimeInterval));
        this->timer.async_wait(this->sendFanPWMFunc);
    };

    io.post([&]() {
        this->timer.async_wait(this->sendFanPWMFunc);
    });

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
PwmSensor::~PwmSensor()
{
    objectServer.remove_interface(sensorInterface);
    objectServer.remove_interface(controlInterface);
    objectServer.remove_interface(valueMutabilityInterface);
}

/**
 * @brief Get the Rde device object path for the pwm sensor
 *
 * @return std::string (Rde device object path)
 */

std::string& PwmSensor::getRdeDeviceObjPath()
{
    return rdeDevObjPath;
}
