#pragma once

#include "libpldm/platform.h"
#include "libpldm/requester/pldm.h"

#include "common/types.hpp"

#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Availability/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

namespace pldm
{
namespace platform_mc
{

using namespace std::chrono;
using SensorUnit = sdbusplus::xyz::openbmc_project::Sensor::server::Value::Unit;
using ValueIntf = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Sensor::server::Value>;
using OperationalStatusIntf =
    sdbusplus::server::object::object<sdbusplus::xyz::openbmc_project::State::
                                          Decorator::server::OperationalStatus>;
using AvailabilityIntf = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::State::Decorator::server::Availability>;
using AssociationDefinitionsInft = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

/**
 * @brief NumericSensor
 *
 * This class handles sensor reading updated by sensor manager and export
 * status to D-Bus interface.
 */
class NumericSensor
{
  public:
    NumericSensor(const uint8_t eid, const uint8_t tid,
                  const bool sensorDisabled,
                  std::shared_ptr<pldm_numeric_sensor_value_pdr> pdr,
                  std::string& sensorName, std::string& associationPath);
    ~NumericSensor(){};

    uint8_t eid;
    uint8_t tid;
    uint16_t sensorId;
    uint32_t elapsedTime;
    uint32_t updateTime;

    void handleRespGetSensorReading(mctp_eid_t eid, const pldm_msg* response,
                                    size_t respMsgLen);
    void handleErrGetSensorReading();

  private:
    void updateReading(bool available, bool functional, double value = 0);

    std::shared_ptr<ValueIntf> valueIntf = nullptr;
    std::shared_ptr<AvailabilityIntf> availabilityIntf = nullptr;
    std::shared_ptr<OperationalStatusIntf> operationalStatusIntf = nullptr;
    std::shared_ptr<AssociationDefinitionsInft> associationDefinitionsIntf =
        nullptr;

    double maxValue;
    double minValue;
    std::shared_ptr<pldm_numeric_sensor_value_pdr> pdr;
};
} // namespace platform_mc
} // namespace pldm
