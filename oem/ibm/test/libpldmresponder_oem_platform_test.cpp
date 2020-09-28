#include "common/utils.hpp"
#include "libpldmresponder/event_parser.hpp"
#include "libpldmresponder/pdr.hpp"
#include "libpldmresponder/pdr_utils.hpp"
#include "libpldmresponder/platform.hpp"
#include "oem/ibm/libpldmresponder/inband_code_update.hpp"
#include "oem/ibm/libpldmresponder/oem_ibm_handler.hpp"
#include "test/mocked_utils.hpp"

#include <iostream>

using namespace pldm::utils;
using namespace pldm::responder;
using namespace pldm::responder::pdr;
using namespace pldm::responder::pdr_utils;
using namespace pldm::responder::oem_ibm_platform;

class MockCodeUpdate : public CodeUpdate
{
  public:
    MockCodeUpdate(const pldm::utils::DBusHandler* dBusIntf) :
        CodeUpdate(dBusIntf)
    {}

    MOCK_METHOD(void, setVersions, (), (override));
};

TEST(OemSetStateEffecterStatesHandler, testGoodRequest)
{
    uint16_t entityID_ = PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE;
    uint16_t stateSetId_ = PLDM_OEM_IBM_BOOT_STATE;
    uint16_t entityInstance_ = 0;
    uint8_t compSensorCnt_ = 1;
    uint16_t effecterId = 0xA;
    sdbusplus::bus::bus bus(sdbusplus::bus::new_default());
    Requester requester(bus, "/abc/def");

    std::vector<get_sensor_state_field> stateField;

    auto mockDbusHandler = std::make_unique<MockdBusHandler>();
    std::unique_ptr<CodeUpdate> mockCodeUpdate =
        std::make_unique<MockCodeUpdate>(mockDbusHandler.get());
    std::unique_ptr<oem_platform::Handler> oemPlatformHandler{};

    oemPlatformHandler = std::make_unique<oem_ibm_platform::Handler>(
        mockDbusHandler.get(), mockCodeUpdate.get(), 0x1, 0x9, requester);

    auto rc = oemPlatformHandler->getOemStateSensorReadingsHandler(
        entityID_, entityInstance_, stateSetId_, compSensorCnt_, stateField);

    ASSERT_EQ(rc, PLDM_SUCCESS);
    ASSERT_EQ(stateField.size(), 1);
    ASSERT_EQ(stateField[0].event_state, T_SIDE);
    ASSERT_EQ(stateField[0].sensor_op_state, PLDM_SENSOR_ENABLED);
    ASSERT_EQ(stateField[0].present_state, PLDM_SENSOR_UNKNOWN);
    ASSERT_EQ(stateField[0].previous_state, PLDM_SENSOR_UNKNOWN);

    entityInstance_ = 1;

    std::vector<get_sensor_state_field> stateField1;
    rc = oemPlatformHandler->getOemStateSensorReadingsHandler(
        entityID_, entityInstance_, stateSetId_, compSensorCnt_, stateField1);
    ASSERT_EQ(stateField1.size(), 1);
    ASSERT_EQ(stateField1[0].event_state, T_SIDE);

    entityInstance_ = 2;
    rc = oemPlatformHandler->getOemStateSensorReadingsHandler(
        entityID_, entityInstance_, stateSetId_, compSensorCnt_, stateField1);
    ASSERT_EQ(stateField1[0].event_state, PLDM_SENSOR_UNKNOWN);

    entityID_ = 40;
    stateSetId_ = 50;
    rc = oemPlatformHandler->getOemStateSensorReadingsHandler(
        entityID_, entityInstance_, stateSetId_, compSensorCnt_, stateField1);
    ASSERT_EQ(rc, PLDM_PLATFORM_INVALID_STATE_VALUE);

    entityID_ = PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE;
    entityInstance_ = 0;
    stateSetId_ = PLDM_OEM_IBM_BOOT_STATE;
    compSensorCnt_ = 1;

    std::vector<set_effecter_state_field> setEffecterStateField;
    setEffecterStateField.push_back({PLDM_REQUEST_SET, P_SIDE});

    rc = oemPlatformHandler->OemSetStateEffecterStatesHandler(
        entityID_, entityInstance_, stateSetId_, compSensorCnt_,
        setEffecterStateField, effecterId);
    ASSERT_EQ(rc, PLDM_SUCCESS);

    entityInstance_ = 2;
    rc = oemPlatformHandler->OemSetStateEffecterStatesHandler(
        entityID_, entityInstance_, stateSetId_, compSensorCnt_,
        setEffecterStateField, effecterId);

    ASSERT_EQ(rc, PLDM_PLATFORM_INVALID_STATE_VALUE);

    entityID_ = 34;
    stateSetId_ = 99;
    entityInstance_ = 0;
    rc = oemPlatformHandler->OemSetStateEffecterStatesHandler(
        entityID_, entityInstance_, stateSetId_, compSensorCnt_,
        setEffecterStateField, effecterId);
    ASSERT_EQ(rc, PLDM_PLATFORM_SET_EFFECTER_UNSUPPORTED_SENSORSTATE);
}

TEST(EncodeCodeUpdateEvent, testGoodRequest)
{
    size_t sensorEventSize = PLDM_SENSOR_EVENT_DATA_MIN_LENGTH + 1;
    std::vector<uint8_t> sensorEventDataVec{};
    sensorEventDataVec.resize(sensorEventSize);

    auto eventData = reinterpret_cast<struct pldm_sensor_event_data*>(
        sensorEventDataVec.data());
    eventData->sensor_id = 0xA;
    eventData->sensor_event_class_type = PLDM_SENSOR_OP_STATE;

    auto opStateSensorEventData =
        reinterpret_cast<struct pldm_sensor_event_sensor_op_state*>(
            sensorEventDataVec.data());
    opStateSensorEventData->present_op_state = START;
    opStateSensorEventData->previous_op_state = END;

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
