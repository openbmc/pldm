#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "mock_sensor_manager.hpp"
#include "platform-mc/terminus_manager.hpp"
#include "test/test_instance_id.hpp"

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
        event(sdeventplus::Event::get_default()), instanceIdDb(),
        reqHandler(pldmTransport, event, instanceIdDb, false),
        terminusManager(event, reqHandler, instanceIdDb, termini, nullptr),
        sensorManager(event, terminusManager, termini)
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

    PldmTransport* pldmTransport = nullptr;
    sdbusplus::bus::bus& bus;
    sdeventplus::Event event;
    TestInstanceIdDb instanceIdDb;
    pldm::requester::Handler<pldm::requester::Request> reqHandler;
    pldm::platform_mc::TerminusManager terminusManager;
    pldm::platform_mc::MockSensorManager sensorManager;
    std::map<pldm::tid_t, std::shared_ptr<pldm::platform_mc::Terminus>> termini;
};

TEST_F(SensorManagerTest, sensorPollingTest)
{
    uint64_t seconds = 10;
    uint64_t expectedTimes = (seconds * 1000) / SENSOR_POLLING_TIME;

    pldm::tid_t tid = 1;
    termini[tid] = std::make_shared<pldm::platform_mc::Terminus>(tid, 0);

    EXPECT_CALL(sensorManager, doSensorPolling(tid))
        .Times(Between(expectedTimes - 3, expectedTimes + 3))
        .WillRepeatedly(Return());
    sensorManager.startPolling(tid);
    runEventLoopForSeconds(seconds);
}
