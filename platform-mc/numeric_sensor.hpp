#pragma once

#include "common/types.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/handler.hpp"

#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Availability/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

using namespace std::chrono;

namespace pldm
{
namespace platform_mc
{

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

struct NumericSensorInfo
{
    std::string sensorName;
    uint16_t sensorId;
    uint8_t sensorDataSize;
    double max;
    double min;
    uint8_t baseUnit;
    float resolution;
    float offset;
    float unitModifier;
};

/**
 * @brief NumericSensor
 *
 * This class registers phoshor-dbus-interfaces to export the status of numeric
 * sensor to Dbus.
 */
class NumericSensor
{
  public:
    // todo: get sensorName from aux name PDR
    NumericSensor(const uint8_t eid, const uint8_t tid,
                  const bool sensorDisabled, const NumericSensorInfo& info,
                  std::string& associationPath);
    ~NumericSensor(){};

    uint8_t eid;
    uint8_t tid;
    uint16_t sensorId;

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

    float resolution;
    float offset;
    int8_t unitModifier;
};
} // namespace platform_mc
} // namespace pldm