#pragma once

#include "libpldm/platform.h"

#include "dbus_impl_requester.hpp"
#include "libpldmresponder/pdr_utils.hpp"

#include <map>

using namespace pldm::dbus_api;
using namespace pldm::responder;

namespace pldm
{

using SensorId = uint16_t;
using DbusObjMaps =
    std::map<SensorId,
             std::tuple<pdr_utils::DbusMappings, pdr_utils::DbusValMaps>>;
using sensorEvent = std::function<void(const uint8_t& sensorId)>;

/** @class DbusToSensorEventHandler
 *  @brief This class can listen to the state sensor PDRs and send PLDM event
 *         msg when a D-Bus property changes
 */
class DbusToSensorEventHandler
{
  public:
    DbusToSensorEventHandler() = delete;
    DbusToSensorEventHandler(const DbusToSensorEventHandler&) = delete;
    DbusToSensorEventHandler(DbusToSensorEventHandler&&) = delete;
    DbusToSensorEventHandler&
        operator=(const DbusToSensorEventHandler&) = delete;
    DbusToSensorEventHandler& operator=(DbusToSensorEventHandler&&) = delete;
    ~DbusToSensorEventHandler() = default;

    /** @brief Constructor
     *  @param[in] mctp_fd - fd of MCTP communications socket
     *  @param[in] mctp_eid - MCTP EID of host firmware
     *  @param[in] requester - reference to Requester object
     */
    explicit DbusToSensorEventHandler(int mctp_fd, uint8_t mctp_eid,
                                      Requester& requester);

    /** @brief Listen all of the state sensor PDRs
     *  @param[in] repo - pdr utils repo object
     *  @param[in] sensorDbusObjMaps - The map of D-Bus mapping and value
     */
    void listenSensorEvent(pdr_utils::Repo repo, DbusObjMaps sensorDbusObjMaps);

  private:
    /** @brief Send state sensor event msg when a D-Bus property changes
     *  @param[in] sensorId - sensor id
     */
    void sendStateSensorEvent(const uint8_t sensorId);

    /** @brief Send all of sensor event
     *  @param[in] eventType - PLDM Event types
     *  @param[in] eventData - send event data
     *  @param[in] eventSize - send event data size
     */
    void sendEventMsg(const uint8_t eventType, const uint8_t* eventData,
                      size_t eventSize);

    /** @brief fd of MCTP communications socket */
    int mctp_fd;

    /** @brief MCTP EID of host firmware */
    uint8_t mctp_eid;

    /** @brief pointer to BMC's primary PDR repo, host PDRs are added here */
    pldm_pdr* repo;

    /** @brief reference to Requester object, primarily used to access API to
     *  obtain PLDM instance id.
     */
    Requester& requester;

    /** @brief D-Bus property changed signal match */
    std::unique_ptr<sdbusplus::bus::match::match> stateSensorMatch;

    /** @brief Sensor D-Bus Maps */
    DbusObjMaps sensorDbusMaps;
};

} // namespace pldm
