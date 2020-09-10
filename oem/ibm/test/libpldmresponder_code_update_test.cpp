#include "libpldmresponder/code_update_event.hpp"

#include <gtest/gtest.h>

using namespace pldm::code_update_event;

TEST(EncodeCodeUpdate, testGoodRequest)
{
    size_t sensorEventSize = PLDM_SENSOR_EVENT_DATA_MIN_LENGTH + 1;
    std::vector<uint8_t> sensorEventDataVec{};
    sensorEventDataVec.resize(sensorEventSize);

    auto eventData = reinterpret_cast<struct pldm_sensor_event_data*>(
        sensorEventDataVec.data());
    eventData->sensor_id = 12;
    eventData->sensor_event_class_type = PLDM_STATE_SENSOR_STATE;
    uint8_t stateSenserEventPtr = 0x0;

    auto stateSensorEventData =
        reinterpret_cast<struct pldm_sensor_event_state_sensor_state*>(
            sensorEventDataVec.data());
    stateSensorEventData->sensor_offset = stateSenserEventPtr;
    stateSensorEventData->event_state = END;
    stateSensorEventData->previous_event_state = START;

    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES +
                                    sensorEventDataVec.size());
    auto rc =
        encodeEventMsg(PLDM_SENSOR_EVENT, sensorEventDataVec, requestMsg, 0x1);

    EXPECT_EQ(rc, PLDM_SUCCESS);
}

TEST(EncodeCodeUpdate, testBadRequest)
{
    std::vector<uint8_t> requestMsg;

    std::vector<uint8_t> sensorEventDataVec{};

    auto rc =
        encodeEventMsg(PLDM_SENSOR_EVENT, sensorEventDataVec, requestMsg, 0x1);

    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}
