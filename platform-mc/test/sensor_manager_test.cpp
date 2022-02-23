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


    EXPECT_CALL(sensorManager, doSensorPolling())
        .Times(Between(9, 11))
        .WillRepeatedly(Return());
    sensorManager.startPolling();
    runEventLoopForSeconds(11);
}