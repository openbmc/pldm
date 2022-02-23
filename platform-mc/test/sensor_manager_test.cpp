#include "mock_sensor_manager.hpp"

#include <sdeventplus/event.hpp>

#include <gtest/gtest.h>

using namespace std::chrono;

using ::testing::_;
using ::testing::Between;
using ::testing::Return;

class SensorManagerTest : public testing::Test
{
  protected:
    SensorManagerTest() :
        bus(pldm::utils::DBusHandler::getBus()),
        event(sdeventplus::Event::get_default()),
        dbusImplRequester(bus, "/xyz/openbmc_project/pldm"),
        reqHandler(fd, event, dbusImplRequester, 1000, false),
        sensorManager(event, reqHandler, dbusImplRequester, termini)
    {}

    void runEventLoopForSeconds(uint64_t sec)
    {
        uint64_t t0 = 0;
        uint64_t t1 = 0;
        uint64_t usec = sec * 1000000;
        uint64_t elapsed = 0;
        sd_event_now(event.get(), CLOCK_MONOTONIC, &t0);
        do
        {
            if (!sd_event_run(event.get(), usec - elapsed))
            {
                break;
            }
            sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
            elapsed = t1 - t0;
        } while (elapsed < usec);
    }

    int fd = -1;
    sdbusplus::bus::bus& bus;
    sdeventplus::Event event;
    pldm::dbus_api::Requester dbusImplRequester;
    pldm::requester::Handler<pldm::requester::Request> reqHandler;
    std::map<mctp_eid_t, std::shared_ptr<pldm::platform_mc::Terminus>> termini;
    pldm::platform_mc::MockSensorManager sensorManager;
};

TEST_F(SensorManagerTest, sensorPollingTest)
{
    // pdr1 set its update_interval to 2 seconds. So sendGetSensorReading()
    // should be called for polling pdr1 sensor total 5 times in 10 seconds.

    mctp_eid_t eid = 1;
    uint64_t supportedTypes = PLDM_BASE | PLDM_PLATFORM;
    auto terminus =
        std::make_shared<pldm::platform_mc::Terminus>(eid, 1, supportedTypes);

    auto pdr1 = std::make_shared<pldm_numeric_sensor_value_pdr>();
    pdr1->base_unit = PLDM_SENSOR_UNIT_DEGRESS_C;
    pdr1->sensor_data_size = PLDM_SENSOR_DATA_SIZE_UINT8;
    pdr1->update_interval = 2;

    terminus->addNumericSensor(pdr1);
    termini[eid] = terminus;

    EXPECT_CALL(sensorManager, sendGetSensorReading(_))
        .Times(Between(4, 6))
        .WillRepeatedly(Return());
    sensorManager.startPolling();
    runEventLoopForSeconds(10);
}