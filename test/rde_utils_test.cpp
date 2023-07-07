#include "rde_fan_sensor/RdeUtils.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/test/sdbus_mock.hpp>

#include <array>
#include <string_view>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Eq;
using ::testing::MatcherCast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SafeMatcherCast;
using ::testing::StrEq;

class TestRdeUtils : public ::testing::Test
{
  public:
    TestRdeUtils() : bus(sdbusplus::get_mocked_new(&sdbusMock))
    {}

    ~TestRdeUtils()
    {}

  protected:
    const std::string errorResponse = "ERROR";
    const std::string successResponse =
        "{\"@odata.id\": \"/redfish/v1/UpdateService\"}";
    const int defaultSlot = 0xdefa;
    sdbusplus::SdBusMock sdbusMock;
    sdbusplus::bus_t bus;

    void MockDbusCallsForDbusMatcher(const std::string& matchRule,
                                     bool injectError)
    {
        ::testing::InSequence seq;
        EXPECT_CALL(sdbusMock, sd_bus_add_match(_, _, StrEq(matchRule), _, _))
            .WillOnce(Return(injectError ? -1 : 0));

        if (!injectError)
        {
            EXPECT_CALL(sdbusMock, sd_bus_slot_unref(_))
                .WillOnce(Return((sd_bus_slot*)&defaultSlot));
        }
    }

    void MockDbusCallsForSendRdeDbusRequestApi(
        const std::string& destination, const std::string& path,
        const char* interface, const std::string& member,
        const pldm_rde_operation_type operationType, const std::string& uri,
        const std::string& rdeDeviceId, const std::string& requestPayload,
        bool injectError)
    {
        ::testing::InSequence seq;
        EXPECT_CALL(sdbusMock, sd_bus_message_new_method_call(
                                   _, _, StrEq(destination), StrEq(path),
                                   StrEq(interface), StrEq(member)))
            .WillOnce(Return(0));

        EXPECT_CALL(sdbusMock,
                    sd_bus_message_append_basic(nullptr, SD_BUS_TYPE_INT32, _))
            .WillOnce(Return(0));

        EXPECT_CALL(sdbusMock, sd_bus_message_append_basic(
                                   nullptr, SD_BUS_TYPE_BYTE,
                                   MatcherCast<const void*>(
                                       SafeMatcherCast<const uint8_t*>(
                                           Pointee(Eq(operationType))))))
            .WillOnce(Return(0));

        EXPECT_CALL(sdbusMock,
                    sd_bus_message_append_basic(
                        nullptr, SD_BUS_TYPE_STRING,
                        MatcherCast<const void*>(
                            SafeMatcherCast<const char*>(StrEq(uri)))))
            .WillOnce(Return(0));

        EXPECT_CALL(sdbusMock,
                    sd_bus_message_append_basic(
                        nullptr, SD_BUS_TYPE_STRING,
                        MatcherCast<const void*>(
                            SafeMatcherCast<const char*>(StrEq(rdeDeviceId)))))
            .WillOnce(Return(0));

        EXPECT_CALL(sdbusMock,
                    sd_bus_message_append_basic(
                        nullptr, SD_BUS_TYPE_STRING,
                        MatcherCast<const void*>(SafeMatcherCast<const char*>(
                            StrEq(requestPayload)))))
            .WillOnce(Return(0));

        EXPECT_CALL(sdbusMock, sd_bus_call(nullptr, nullptr, 0, _, _))
            .WillOnce(Return(injectError ? -1 : 0));

        if (!injectError)
        {
            EXPECT_CALL(sdbusMock, sd_bus_message_read_basic(nullptr, _, _))
                .WillOnce(Return(0));
        }
    }
};

TEST_F(TestRdeUtils, isResponseErrorTest)
{
    EXPECT_TRUE(isResponseError(errorResponse));
    EXPECT_FALSE(isResponseError(successResponse));
}

TEST_F(TestRdeUtils, setupRdeDeviceDetectionMatchesTest)
{
    std::function<void(sdbusplus::message_t&)>
        rdeDeviceDetetectionMatchCallback = [&](sdbusplus::message_t& message) {
            if (message.is_method_error())
            {
                std::cerr << "callback method error\n";
                return;
            }
        };

    std::string rdeDetectionMatchRule =
        sdbusplus::bus::match::rules::propertiesChangedNamespace(
            rdeDevicesPath, rdeDeviceInterface);

    // Test the success case
    {
        ::testing::InSequence seq;

        MockDbusCallsForDbusMatcher(rdeDetectionMatchRule, false);
        std::vector<std::unique_ptr<sdbusplus::bus::match_t>> detectionMatches =
            setupRdeDeviceDetectionMatches(bus,
                                           rdeDeviceDetetectionMatchCallback);

        EXPECT_FALSE(detectionMatches.empty());
    }

    // Test the failure case
    {
        ::testing::InSequence seq;

        MockDbusCallsForDbusMatcher(rdeDetectionMatchRule, true);
        // An exception is raised if D-Bus matcher is not created successfully
        EXPECT_THROW(setupRdeDeviceDetectionMatches(
                         bus, rdeDeviceDetetectionMatchCallback),
                     std::exception);
    }
}

TEST_F(TestRdeUtils, setupRdeDeviceRemovalMatchesTest)
{
    std::function<void(sdbusplus::message_t&)> rdeDeviceRemovalMatchCallback =
        [&](sdbusplus::message_t& message) {
            if (message.is_method_error())
            {
                std::cerr << "callback method error\n";
                return;
            }
        };

    std::string rdeRemovalMatchRule =
        sdbusplus::bus::match::rules::interfacesRemoved() +
        sdbusplus::bus::match::rules::argNpath(0, std::string(rdeDevicesPath) +
                                                      "/");

    // Test the success case
    {
        ::testing::InSequence seq;

        MockDbusCallsForDbusMatcher(rdeRemovalMatchRule, false);
        std::unique_ptr<sdbusplus::bus::match_t> removalMatch =
            setupRdeDeviceRemovalMatches(bus, rdeDeviceRemovalMatchCallback);
        EXPECT_FALSE(removalMatch == nullptr);
    }

    // Test the failure case
    {
        ::testing::InSequence seq;

        MockDbusCallsForDbusMatcher(rdeRemovalMatchRule, true);
        // An exception is raised if D-Bus matcher is not created successfully
        EXPECT_THROW(
            setupRdeDeviceRemovalMatches(bus, rdeDeviceRemovalMatchCallback),
            std::exception);
    }
}

TEST_F(TestRdeUtils, sendRdeDbusRequestTest)
{
    std::string uri = "/redfish/v1/Chassis/RdeTRAY/Sensors/fan0_duty";
    std::string rdeDeviceId = "1_1_4_1";
    std::string rdeServiceName = "xyz.openbmc_project.rdeoperation";
    std::string rdeObjPath = "/xyz/openbmc_project/rde_devices/1_1_4_1";
    pldm_rde_operation_type operationType = PLDM_RDE_OPERATION_READ;
    std::string member = "execute_rde";
    std::string requestPayload = "default";

    // Test the success case
    {
        ::testing::InSequence seq;
        MockDbusCallsForSendRdeDbusRequestApi(
            rdeServiceName, rdeObjPath, rdeDeviceInterface, member,
            operationType, uri, rdeDeviceId, requestPayload, false);
        // Expect std::logic_error exception, as the mock
        // sd_bus_message_read_basic API cannot construct a response string with
        // nullptr
        EXPECT_THROW(sendRdeDbusRequest(bus, uri, rdeDeviceId, rdeServiceName,
                                        rdeObjPath, operationType,
                                        requestPayload),
                     std::logic_error);
    }

    // Test the failure case
    {
        ::testing::InSequence seq;
        MockDbusCallsForSendRdeDbusRequestApi(
            rdeServiceName, rdeObjPath, rdeDeviceInterface, member,
            operationType, uri, rdeDeviceId, requestPayload, true);
        EXPECT_THAT(sendRdeDbusRequest(bus, uri, rdeDeviceId, rdeServiceName,
                                       rdeObjPath, operationType,
                                       requestPayload),
                    errorResponse);
    }
}
