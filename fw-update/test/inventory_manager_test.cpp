#include "common/start_lifetime_as.hpp"
#include "common/utils.hpp"
#include "fw-update/aggregate_update_manager.hpp"
#include "fw-update/inventory_manager.hpp"
#include "test/test_instance_id.hpp"

#include <libpldm/firmware_update.h>

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace pldm;
using namespace std::chrono;
using namespace pldm::fw_update;

class InventoryManagerTest : public testing::Test
{
  protected:
    InventoryManagerTest() :
        event(sdeventplus::Event::get_default()), instanceIdDb(),
        reqHandler(nullptr, event, instanceIdDb, false, seconds(1), 2,
                   milliseconds(100)),
        updateManager(event, reqHandler, instanceIdDb, outDescriptorMap,
                      outComponentInfoMap),
        inventoryManager(&dBusHandler, reqHandler, instanceIdDb,
                         outDescriptorMap, outDownstreamDescriptorMap,
                         outComponentInfoMap, configurations, updateManager)
    {}

    int fd = -1;
    const pldm::utils::DBusHandler dBusHandler;
    sdeventplus::Event event;
    TestInstanceIdDb instanceIdDb;
    requester::Handler<requester::Request> reqHandler;
    AggregateUpdateManager updateManager;
    InventoryManager inventoryManager;
    DescriptorMap outDescriptorMap{};
    DownstreamDescriptorMap outDownstreamDescriptorMap{};
    ComponentInfoMap outComponentInfoMap{};
    Configurations configurations;

    /** @brief Make the FD look as if QueryDeviceIdentifiers already
     *         completed, so GetFirmwareParameters creates inventory entries.
     */
    void addFirmwareDevice(pldm::eid eid, const SoftwareName& name)
    {
        inventoryManager.firmwareDeviceNameMap.insert_or_assign(eid, name);
        outDescriptorMap.insert_or_assign(
            eid, Descriptors{{PLDM_FWUP_IANA_ENTERPRISE_ID,
                              std::vector<uint8_t>{0x0a, 0x0b, 0x0c, 0x0d}}});
    }

    const SoftwareMap& softwareMap() const
    {
        return inventoryManager.firmwareInventoryManager.softwareMap;
    }
};

namespace
{

struct TestComponent
{
    uint16_t classification;
    uint16_t identifier;
    uint8_t classificationIndex;
    std::string activeVersion;
};

/** @brief Build a GetFirmwareParameters response (DSP0267) with
 *         an ASCII active image set version, no pending versions and the
 *         given component parameter table.
 */
std::vector<uint8_t> buildGetFirmwareParametersResp(
    const std::string& imageSetVersion,
    const std::vector<TestComponent>& components)
{
    std::vector<uint8_t> buf(sizeof(pldm_msg_hdr), 0);

    auto put8 = [&buf](uint8_t v) { buf.push_back(v); };
    auto put16 = [&put8](uint16_t v) {
        put8(static_cast<uint8_t>(v & 0xff));
        put8(static_cast<uint8_t>(v >> 8));
    };
    auto put32 = [&put16](uint32_t v) {
        put16(static_cast<uint16_t>(v & 0xffff));
        put16(static_cast<uint16_t>(v >> 16));
    };
    auto putStr = [&buf](const std::string& str) {
        buf.insert(buf.end(), str.begin(), str.end());
    };
    auto putZeros = [&buf](size_t n) { buf.insert(buf.end(), n, 0); };

    put8(PLDM_SUCCESS);                              // CompletionCode
    put32(0);                                        // Capabilities
    put16(static_cast<uint16_t>(components.size())); // ComponentCount
    put8(PLDM_STR_TYPE_ASCII);
    put8(static_cast<uint8_t>(imageSetVersion.size()));
    put8(PLDM_STR_TYPE_UNKNOWN); // no pending image set version
    put8(0);
    putStr(imageSetVersion);

    for (const auto& comp : components)
    {
        put16(comp.classification);
        put16(comp.identifier);
        put8(comp.classificationIndex);
        put32(0); // ActiveComponentComparisonStamp
        put8(PLDM_STR_TYPE_ASCII);
        put8(static_cast<uint8_t>(comp.activeVersion.size()));
        putZeros(8); // ActiveComponentReleaseDate
        put32(0);    // PendingComponentComparisonStamp
        put8(PLDM_STR_TYPE_UNKNOWN);
        put8(0);
        putZeros(8); // PendingComponentReleaseDate
        put16(0);    // ComponentActivationMethods
        put32(0);    // CapabilitiesDuringUpdate
        putStr(comp.activeVersion);
    }

    return buf;
}

const pldm_msg* asMsg(const std::vector<uint8_t>& buf)
{
    return reinterpret_cast<const pldm_msg*>(buf.data());
}

size_t payloadLen(const std::vector<uint8_t>& buf)
{
    return buf.size() - sizeof(pldm_msg_hdr);
}

} // namespace

TEST_F(InventoryManagerTest, handleQueryDeviceIdentifiersResponse)
{
    constexpr size_t respPayloadLength1 = 49;
    constexpr std::array<uint8_t, sizeof(pldm_msg_hdr) + respPayloadLength1>
        queryDeviceIdentifiersResp1{
            0x00, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x00, 0x00, 0x03, 0x01, 0x00,
            0x04, 0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x02, 0x00, 0x10, 0x00, 0x12,
            0x44, 0xd2, 0x64, 0x8d, 0x7d, 0x47, 0x18, 0xa0, 0x30, 0xfc, 0x8a,
            0x56, 0x58, 0x7d, 0x5b, 0xFF, 0xFF, 0x0B, 0x00, 0x01, 0x07, 0x4f,
            0x70, 0x65, 0x6e, 0x42, 0x4d, 0x43, 0x01, 0x02};
    auto responseMsg1 =
        reinterpret_cast<const pldm_msg*>(queryDeviceIdentifiersResp1.data());
    inventoryManager.queryDeviceIdentifiers(1, responseMsg1,
                                            respPayloadLength1);

    DescriptorMap descriptorMap1{
        {0x01,
         {{PLDM_FWUP_IANA_ENTERPRISE_ID,
           std::vector<uint8_t>{0x0a, 0x0b, 0x0c, 0xd}},
          {PLDM_FWUP_UUID,
           std::vector<uint8_t>{0x12, 0x44, 0xd2, 0x64, 0x8d, 0x7d, 0x47, 0x18,
                                0xa0, 0x30, 0xfc, 0x8a, 0x56, 0x58, 0x7d,
                                0x5b}},
          {PLDM_FWUP_VENDOR_DEFINED,
           std::make_tuple("OpenBMC", std::vector<uint8_t>{0x01, 0x02})}}}};

    EXPECT_EQ(outDescriptorMap.size(), descriptorMap1.size());
    EXPECT_EQ(outDescriptorMap, descriptorMap1);

    constexpr size_t respPayloadLength2 = 26;
    constexpr std::array<uint8_t, sizeof(pldm_msg_hdr) + respPayloadLength2>
        queryDeviceIdentifiersResp2{
            0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x01, 0x02,
            0x00, 0x10, 0x00, 0xF0, 0x18, 0x87, 0x8C, 0xCB, 0x7D, 0x49,
            0x43, 0x98, 0x00, 0xA0, 0x2F, 0x59, 0x9A, 0xCA, 0x02};
    auto responseMsg2 =
        reinterpret_cast<const pldm_msg*>(queryDeviceIdentifiersResp2.data());
    inventoryManager.queryDeviceIdentifiers(2, responseMsg2,
                                            respPayloadLength2);
    DescriptorMap descriptorMap2{
        {0x01,
         {{PLDM_FWUP_IANA_ENTERPRISE_ID,
           std::vector<uint8_t>{0x0a, 0x0b, 0x0c, 0xd}},
          {PLDM_FWUP_UUID,
           std::vector<uint8_t>{0x12, 0x44, 0xd2, 0x64, 0x8d, 0x7d, 0x47, 0x18,
                                0xa0, 0x30, 0xfc, 0x8a, 0x56, 0x58, 0x7d,
                                0x5b}},
          {PLDM_FWUP_VENDOR_DEFINED,
           std::make_tuple("OpenBMC", std::vector<uint8_t>{0x01, 0x02})}}},
        {0x02,
         {{PLDM_FWUP_UUID,
           std::vector<uint8_t>{0xF0, 0x18, 0x87, 0x8C, 0xCB, 0x7D, 0x49, 0x43,
                                0x98, 0x00, 0xA0, 0x2F, 0x59, 0x9A, 0xCA,
                                0x02}}}}};
    EXPECT_EQ(outDescriptorMap.size(), descriptorMap2.size());
    EXPECT_EQ(outDescriptorMap, descriptorMap2);
}

TEST_F(InventoryManagerTest, handleQueryDeviceIdentifiersResponseErrorCC)
{
    constexpr size_t respPayloadLength = 1;
    constexpr std::array<uint8_t, sizeof(pldm_msg_hdr) + respPayloadLength>
        queryDeviceIdentifiersResp{0x00, 0x00, 0x00, 0x01};
    auto responseMsg =
        reinterpret_cast<const pldm_msg*>(queryDeviceIdentifiersResp.data());
    inventoryManager.queryDeviceIdentifiers(1, responseMsg, respPayloadLength);
    EXPECT_EQ(outDescriptorMap.size(), 0);
}

TEST_F(InventoryManagerTest, handleQueryDownstreamIdentifierResponse)
{
    constexpr uint8_t eid = 1;
    constexpr uint8_t downstreamDeviceCount = 1;
    constexpr uint32_t downstreamDeviceLen = 11;
    constexpr size_t respPayloadLength =
        PLDM_QUERY_DOWNSTREAM_IDENTIFIERS_RESP_MIN_LEN + downstreamDeviceLen;

    std::array<uint8_t, sizeof(pldm_msg_hdr) + respPayloadLength>
        queryDownstreamIdentifiersResp{
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
            0x0B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
            0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0xa0, 0x15};
    auto responseMsg = new (queryDownstreamIdentifiersResp.data()) pldm_msg;

    inventoryManager.queryDownstreamIdentifiers(eid, responseMsg,
                                                respPayloadLength);

    DownstreamDeviceInfo downstreamDevices = {
        {0,
         {{PLDM_FWUP_IANA_ENTERPRISE_ID,
           std::vector<uint8_t>{0x00, 0x00, 0xa0, 0x15}}}}};
    DownstreamDescriptorMap refDownstreamDescriptorMap{
        {eid, downstreamDevices}};

    ASSERT_EQ(outDownstreamDescriptorMap.size(), downstreamDeviceCount);
    ASSERT_EQ(outDownstreamDescriptorMap.size(),
              refDownstreamDescriptorMap.size());
    ASSERT_EQ(outDownstreamDescriptorMap, refDownstreamDescriptorMap);
}

TEST_F(InventoryManagerTest, handleQueryDownstreamIdentifierResponseErrorCC)
{
    constexpr size_t respPayloadLength = 1;
    constexpr std::array<uint8_t, sizeof(pldm_msg_hdr) + respPayloadLength>
        queryDownstreamIdentifiersResp{0x00, 0x00, 0x00, 0x01};
    const auto responseMsg = std::start_lifetime_as<pldm_msg>(
        const_cast<unsigned char*>(queryDownstreamIdentifiersResp.data()));
    inventoryManager.queryDownstreamIdentifiers(1, responseMsg,
                                                respPayloadLength);

    ASSERT_EQ(outDownstreamDescriptorMap.size(), 0);
}

TEST_F(InventoryManagerTest, getFirmwareParametersResponse)
{
    // constexpr uint16_t compCount = 2;
    // constexpr std::string_view activeCompImageSetVersion{"DeviceVer1.0"};
    // constexpr std::string_view activeCompVersion1{"Comp1v2.0"};
    // constexpr std::string_view activeCompVersion2{"Comp2v3.0"};
    constexpr uint16_t compClassification1 = 10;
    constexpr uint16_t compIdentifier1 = 300;
    constexpr uint8_t compClassificationIndex1 = 20;
    constexpr uint16_t compClassification2 = 16;
    constexpr uint16_t compIdentifier2 = 301;
    constexpr uint8_t compClassificationIndex2 = 30;

    constexpr size_t respPayloadLength1 = 119;
    constexpr std::array<uint8_t, sizeof(pldm_msg_hdr) + respPayloadLength1>
        getFirmwareParametersResp1{
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01,
            0x0c, 0x00, 0x00, 0x44, 0x65, 0x76, 0x69, 0x63, 0x65, 0x56, 0x65,
            0x72, 0x31, 0x2e, 0x30, 0x0a, 0x00, 0x2c, 0x01, 0x14, 0x00, 0x00,
            0x00, 0x00, 0x01, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x43,
            0x6f, 0x6d, 0x70, 0x31, 0x76, 0x32, 0x2e, 0x30, 0x10, 0x00, 0x2d,
            0x01, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x01, 0x09, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x43, 0x6f, 0x6d, 0x70, 0x32, 0x76, 0x33, 0x2e,
            0x30};
    auto responseMsg1 =
        reinterpret_cast<const pldm_msg*>(getFirmwareParametersResp1.data());
    inventoryManager.getFirmwareParameters(1, responseMsg1, respPayloadLength1);

    ComponentInfoMap componentInfoMap1{
        {1,
         {{std::make_pair(compClassification1, compIdentifier1),
           compClassificationIndex1},
          {std::make_pair(compClassification2, compIdentifier2),
           compClassificationIndex2}}}};
    EXPECT_EQ(outComponentInfoMap.size(), componentInfoMap1.size());
    EXPECT_EQ(outComponentInfoMap, componentInfoMap1);

    // constexpr uint16_t compCount = 1;
    // constexpr std::string_view activeCompImageSetVersion{"DeviceVer2.0"};
    // constexpr std::string_view activeCompVersion1{"Comp3v4.0"};
    constexpr uint16_t compClassification3 = 2;
    constexpr uint16_t compIdentifier3 = 302;
    constexpr uint8_t compClassificationIndex3 = 40;

    constexpr size_t respPayloadLength2 = 119;
    constexpr std::array<uint8_t, sizeof(pldm_msg_hdr) + respPayloadLength2>
        getFirmwareParametersResp2{
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01,
            0x0c, 0x00, 0x00, 0x44, 0x65, 0x76, 0x69, 0x63, 0x65, 0x56, 0x65,
            0x72, 0x32, 0x2e, 0x30, 0x02, 0x00, 0x2e, 0x01, 0x28, 0x00, 0x00,
            0x00, 0x00, 0x01, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x43,
            0x6f, 0x6d, 0x70, 0x33, 0x76, 0x34, 0x2e, 0x30};
    auto responseMsg2 =
        reinterpret_cast<const pldm_msg*>(getFirmwareParametersResp2.data());
    inventoryManager.getFirmwareParameters(2, responseMsg2, respPayloadLength2);

    ComponentInfoMap componentInfoMap2{
        {1,
         {{std::make_pair(compClassification1, compIdentifier1),
           compClassificationIndex1},
          {std::make_pair(compClassification2, compIdentifier2),
           compClassificationIndex2}}},
        {2,
         {{std::make_pair(compClassification3, compIdentifier3),
           compClassificationIndex3}}}};
    EXPECT_EQ(outComponentInfoMap.size(), componentInfoMap2.size());
    EXPECT_EQ(outComponentInfoMap, componentInfoMap2);
}

TEST_F(InventoryManagerTest, getFirmwareParametersResponseErrorCC)
{
    constexpr size_t respPayloadLength = 1;
    constexpr std::array<uint8_t, sizeof(pldm_msg_hdr) + respPayloadLength>
        getFirmwareParametersResp{0x00, 0x00, 0x00, 0x01};
    auto responseMsg =
        reinterpret_cast<const pldm_msg*>(getFirmwareParametersResp.data());
    inventoryManager.getFirmwareParameters(1, responseMsg, respPayloadLength);
    EXPECT_EQ(outComponentInfoMap.size(), 0);
}

TEST_F(InventoryManagerTest,
       getFirmwareParametersSingleComponentSkipsImageLevelEntry)
{
    // CX7-like FD: the image set version equals the only component version
    constexpr pldm::eid eid = 1;
    constexpr uint16_t compId = 1;
    addFirmwareDevice(eid, "NIC0");

    auto resp = buildGetFirmwareParametersResp("28.98.5416",
                                               {{10, compId, 0, "28.98.5416"}});
    inventoryManager.getFirmwareParameters(eid, asMsg(resp), payloadLen(resp));

    EXPECT_EQ(softwareMap().size(), 1);
    EXPECT_TRUE(softwareMap().contains(SoftwareIdentifier{eid, compId}));
    EXPECT_FALSE(softwareMap().contains(SoftwareIdentifier{eid, std::nullopt}));
    ASSERT_TRUE(outComponentInfoMap.contains(eid));
    EXPECT_EQ(outComponentInfoMap.at(eid).size(), 1);
}

TEST_F(InventoryManagerTest,
       getFirmwareParametersDistinctVersionsCreateImageLevelEntry)
{
    constexpr pldm::eid eid = 1;
    constexpr uint16_t compId1 = 300;
    constexpr uint16_t compId2 = 301;
    addFirmwareDevice(eid, "Device");

    auto resp = buildGetFirmwareParametersResp(
        "DeviceVer1.0",
        {{10, compId1, 20, "Comp1v2.0"}, {16, compId2, 30, "Comp2v3.0"}});
    inventoryManager.getFirmwareParameters(eid, asMsg(resp), payloadLen(resp));

    EXPECT_EQ(softwareMap().size(), 3);
    EXPECT_TRUE(softwareMap().contains(SoftwareIdentifier{eid, std::nullopt}));
    EXPECT_TRUE(softwareMap().contains(SoftwareIdentifier{eid, compId1}));
    EXPECT_TRUE(softwareMap().contains(SoftwareIdentifier{eid, compId2}));
}

TEST_F(InventoryManagerTest,
       getFirmwareParametersMultiComponentMatchSkipsImageLevelEntry)
{
    // The check keys on version equality, not on component count
    constexpr pldm::eid eid = 1;
    constexpr uint16_t compId1 = 300;
    constexpr uint16_t compId2 = 301;
    addFirmwareDevice(eid, "Device");

    auto resp = buildGetFirmwareParametersResp(
        "Comp2v3.0",
        {{10, compId1, 20, "Comp1v2.0"}, {16, compId2, 30, "Comp2v3.0"}});
    inventoryManager.getFirmwareParameters(eid, asMsg(resp), payloadLen(resp));

    EXPECT_EQ(softwareMap().size(), 2);
    EXPECT_FALSE(softwareMap().contains(SoftwareIdentifier{eid, std::nullopt}));
    EXPECT_TRUE(softwareMap().contains(SoftwareIdentifier{eid, compId1}));
    EXPECT_TRUE(softwareMap().contains(SoftwareIdentifier{eid, compId2}));
}

TEST_F(InventoryManagerTest,
       getFirmwareParametersComponentIdZeroDoesNotCollideWithImageLevel)
{
    // CompIdentifier 0 is a valid vendor-assigned value; it must not alias
    // the image-level entry (previously keyed as {eid, 0}).
    constexpr pldm::eid eid = 1;
    constexpr uint16_t compId = 0;
    addFirmwareDevice(eid, "Device");

    auto resp = buildGetFirmwareParametersResp("ImageVer1.0",
                                               {{10, compId, 0, "Comp0v1.0"}});
    inventoryManager.getFirmwareParameters(eid, asMsg(resp), payloadLen(resp));

    EXPECT_EQ(softwareMap().size(), 2);
    EXPECT_TRUE(softwareMap().contains(SoftwareIdentifier{eid, std::nullopt}));
    EXPECT_TRUE(softwareMap().contains(SoftwareIdentifier{eid, compId}));
}

TEST_F(InventoryManagerTest,
       getFirmwareParametersMalformedEntryKeepsParsedComponents)
{
    constexpr pldm::eid eid = 1;
    constexpr uint16_t compClassification1 = 10;
    constexpr uint16_t compId1 = 300;
    constexpr uint8_t compClassificationIndex1 = 20;
    addFirmwareDevice(eid, "Device");

    auto resp = buildGetFirmwareParametersResp(
        "DeviceVer1.0",
        {{compClassification1, compId1, compClassificationIndex1, "Comp1v2.0"},
         {16, 301, 30, "Comp2v3.0"}});
    // Truncate the second component entry so that decoding it fails
    resp.resize(resp.size() - 5);
    inventoryManager.getFirmwareParameters(eid, asMsg(resp), payloadLen(resp));

    ComponentInfoMap expected{{eid,
                               {{std::make_pair(compClassification1, compId1),
                                 compClassificationIndex1}}}};
    EXPECT_EQ(outComponentInfoMap, expected);

    EXPECT_EQ(softwareMap().size(), 2);
    EXPECT_TRUE(softwareMap().contains(SoftwareIdentifier{eid, std::nullopt}));
    EXPECT_TRUE(softwareMap().contains(SoftwareIdentifier{eid, compId1}));
}
