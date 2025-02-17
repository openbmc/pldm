#include "common/instance_id.hpp"
#include "common/utils.hpp"
#include "fw-update/device_updater.hpp"
#include "fw-update/package_parser.hpp"
#include "requester/handler.hpp"

#include <libpldm/firmware_update.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm;
using namespace pldm::fw_update;

class DeviceUpdaterTest : public testing::Test
{
  protected:
    DeviceUpdaterTest() :
        package("./test_pkg", std::ios::binary | std::ios::in | std::ios::ate)
    {
        fwDeviceIDRecord = {
            1,
            {0x00},
            "VersionString2",
            {{PLDM_FWUP_UUID,
              std::vector<uint8_t>{0x16, 0x20, 0x23, 0xC9, 0x3E, 0xC5, 0x41,
                                   0x15, 0x95, 0xF4, 0x48, 0x70, 0x1D, 0x49,
                                   0xD6, 0x75}}},
            {}};
        compImageInfos = {
            {10, 100, 0xFFFFFFFF, 0, 0, 139, 1024, "VersionString3"}};
        compInfo = {{std::make_pair(10, 100), 1}};
    }

    int fd = -1;
    std::ifstream package;
    FirmwareDeviceIDRecord fwDeviceIDRecord;
    ComponentImageInfos compImageInfos;
    ComponentInfo compInfo;
};

TEST_F(DeviceUpdaterTest, validatePackage)
{
    constexpr uintmax_t testPkgSize = 1163;
    uintmax_t packageSize = package.tellg();
    EXPECT_EQ(packageSize, testPkgSize);

    package.seekg(0);
    std::vector<uint8_t> packageHeader(sizeof(pldm_package_header_information));
    package.read(new (packageHeader.data()) char,
                 sizeof(pldm_package_header_information));

    auto pkgHeaderInfo =
        reinterpret_cast<const pldm_package_header_information*>(
            packageHeader.data());
    auto pkgHeaderInfoSize = pkgHeaderInfo->package_header_size;
    ;
    packageHeader.clear();
    packageHeader.resize(pkgHeaderInfoSize);
    package.seekg(0);
    package.read(new (packageHeader.data()) char, pkgHeaderInfoSize);

    parser = std::make_unique<PackageParserV1>();

    parser->parse(packageHeader, packageSize);
    const auto& fwDeviceIDRecords = parser->getFwDeviceIDRecords();
    const auto& testPkgCompImageInfos = parser->getComponentImageInfos();

    EXPECT_EQ(fwDeviceIDRecords.size(), 1);
    EXPECT_EQ(compImageInfos.size(), 1);
    EXPECT_EQ(fwDeviceIDRecords[0], fwDeviceIDRecord);
    EXPECT_EQ(testPkgCompImageInfos, compImageInfos);
}

TEST_F(DeviceUpdaterTest, ReadPackage512B)
{
    DeviceUpdater deviceUpdater(0, package, fwDeviceIDRecord, compImageInfos,
                                compInfo, 512, nullptr);

    constexpr std::array<uint8_t, sizeof(pldm_msg_hdr) +
                                      sizeof(pldm_request_firmware_data_req)>
        reqFwDataReq{0x8A, 0x05, 0x15, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x02, 0x00, 0x00};
    constexpr uint8_t instanceId = 0x0A;
    constexpr uint8_t completionCode = PLDM_SUCCESS;
    constexpr uint32_t length = 512;
    auto requestMsg = reinterpret_cast<const pldm_msg*>(reqFwDataReq.data());
    auto response = deviceUpdater.requestFwData(
        requestMsg, sizeof(pldm_request_firmware_data_req));

    EXPECT_EQ(response.size(),
              sizeof(pldm_msg_hdr) + sizeof(completionCode) + length);
    auto responeMsg = reinterpret_cast<const pldm_msg*>(response.data());
    EXPECT_EQ(responeMsg->hdr.request, PLDM_RESPONSE);
    EXPECT_EQ(responeMsg->hdr.instance_id, instanceId);
    EXPECT_EQ(responeMsg->hdr.type, PLDM_FWUP);
    EXPECT_EQ(responeMsg->hdr.command, PLDM_REQUEST_FIRMWARE_DATA);
    EXPECT_EQ(response[sizeof(pldm_msg_hdr)], completionCode);

    const std::vector<uint8_t> compFirst512B{
        0x0A, 0x05, 0x15, 0x00, 0x48, 0xD2, 0x1E, 0x80, 0x2E, 0x77, 0x71, 0x2C,
        0x8E, 0xE3, 0x1F, 0x6F, 0x30, 0x76, 0x65, 0x08, 0xB8, 0x1B, 0x4B, 0x03,
        0x7E, 0x96, 0xD9, 0x2A, 0x36, 0x3A, 0xA2, 0xEE, 0x8A, 0x30, 0x21, 0x33,
        0xFC, 0x27, 0xE7, 0x3E, 0x56, 0x79, 0x0E, 0xBD, 0xED, 0x44, 0x96, 0x2F,
        0x84, 0xB5, 0xED, 0x19, 0x3A, 0x5E, 0x62, 0x2A, 0x6E, 0x41, 0x7E, 0xDC,
        0x2E, 0xBB, 0x87, 0x41, 0x7F, 0xCE, 0xF0, 0xD7, 0xE4, 0x0F, 0x95, 0x33,
        0x3B, 0xF9, 0x04, 0xF8, 0x1A, 0x92, 0x54, 0xFD, 0x33, 0xBA, 0xCD, 0xA6,
        0x08, 0x0D, 0x32, 0x2C, 0xEB, 0x75, 0xDC, 0xEA, 0xBA, 0x30, 0x94, 0x78,
        0x8C, 0x61, 0x58, 0xD0, 0x59, 0xF3, 0x29, 0x6D, 0x67, 0xD3, 0x26, 0x08,
        0x25, 0x1E, 0x69, 0xBB, 0x28, 0xB0, 0x61, 0xFB, 0x96, 0xA3, 0x8C, 0xBF,
        0x01, 0x94, 0xEB, 0x3A, 0x63, 0x6F, 0xC8, 0x0F, 0x42, 0x7F, 0xEB, 0x3D,
        0xA7, 0x8B, 0xE5, 0xD2, 0xFB, 0xB8, 0xD3, 0x15, 0xAA, 0xDF, 0x86, 0xAB,
        0x6E, 0x29, 0xB3, 0x12, 0x96, 0xB7, 0x86, 0xDA, 0xF9, 0xD7, 0x70, 0xAD,
        0xB6, 0x1A, 0x29, 0xB1, 0xA4, 0x2B, 0x6F, 0x63, 0xEE, 0x05, 0x9F, 0x35,
        0x49, 0xA1, 0xAB, 0xA2, 0x6F, 0x7C, 0xFC, 0x23, 0x09, 0x55, 0xED, 0xF7,
        0x35, 0xD8, 0x2F, 0x8F, 0xD2, 0xBD, 0x77, 0xED, 0x0C, 0x7A, 0xE9, 0xD3,
        0xF7, 0x90, 0xA7, 0x45, 0x97, 0xAA, 0x3A, 0x79, 0xC4, 0xF8, 0xD2, 0xFE,
        0xFB, 0xB3, 0x25, 0x86, 0x98, 0x6B, 0x98, 0x10, 0x15, 0xB3, 0xDD, 0x43,
        0x0B, 0x20, 0x5F, 0xE4, 0x62, 0xC8, 0xA1, 0x3E, 0x9C, 0xF3, 0xD8, 0xEA,
        0x15, 0xA1, 0x24, 0x94, 0x1C, 0xF5, 0xB4, 0x86, 0x04, 0x30, 0x2C, 0x84,
        0xB6, 0x29, 0xF6, 0x9D, 0x76, 0x6E, 0xD4, 0x0C, 0x1C, 0xBD, 0xF9, 0x95,
        0x7E, 0xAF, 0x62, 0x80, 0x14, 0xE6, 0x1C, 0x43, 0x51, 0x5C, 0xCA, 0x50,
        0xE1, 0x73, 0x3D, 0x75, 0x66, 0x52, 0x9E, 0xB6, 0x15, 0x7E, 0xF7, 0xE5,
        0xE2, 0xAF, 0x54, 0x75, 0x82, 0x3D, 0x55, 0xC7, 0x59, 0xD7, 0xBD, 0x8C,
        0x4B, 0x74, 0xD1, 0x3F, 0xA8, 0x1B, 0x0A, 0xF0, 0x5A, 0x32, 0x2B, 0xA7,
        0xA4, 0xBE, 0x38, 0x18, 0xAE, 0x69, 0xDC, 0x54, 0x7C, 0x60, 0xEF, 0x4F,
        0x0F, 0x7F, 0x5A, 0xA6, 0xC8, 0x3E, 0x59, 0xFD, 0xF5, 0x98, 0x26, 0x71,
        0xD0, 0xEF, 0x54, 0x47, 0x38, 0x1F, 0x18, 0x9D, 0x37, 0x9D, 0xF0, 0xCD,
        0x00, 0x73, 0x30, 0xD4, 0xB7, 0xDA, 0x2D, 0x36, 0xA1, 0xA9, 0xAD, 0x4F,
        0x9F, 0x17, 0xA5, 0xA1, 0x62, 0x18, 0x21, 0xDD, 0x0E, 0xB6, 0x72, 0xDE,
        0x17, 0xF0, 0x71, 0x94, 0xA9, 0x67, 0xB4, 0x75, 0xDB, 0x64, 0xF0, 0x6E,
        0x3D, 0x4E, 0x29, 0x45, 0x42, 0xC3, 0xDA, 0x1F, 0x9E, 0x31, 0x4D, 0x1B,
        0xA7, 0x9D, 0x07, 0xD9, 0x10, 0x75, 0x27, 0x92, 0x16, 0x35, 0xF5, 0x51,
        0x3E, 0x14, 0x00, 0xB4, 0xBD, 0x21, 0xAF, 0x90, 0xC5, 0xE5, 0xEE, 0xD0,
        0xB3, 0x7F, 0x61, 0xA5, 0x1B, 0x91, 0xD5, 0x66, 0x08, 0xB5, 0x16, 0x25,
        0xC2, 0x16, 0x53, 0xDC, 0xB5, 0xF1, 0xDD, 0xCF, 0x28, 0xDD, 0x57, 0x90,
        0x66, 0x33, 0x7B, 0x75, 0xF4, 0x8A, 0x19, 0xAC, 0x1F, 0x44, 0xC2, 0xF6,
        0x21, 0x07, 0xE9, 0xCC, 0xDD, 0xCF, 0x4A, 0x34, 0xA1, 0x24, 0x82, 0xF8,
        0xA1, 0x1D, 0x06, 0x90, 0x4B, 0x97, 0xB8, 0x10, 0xF2, 0x6A, 0x55, 0x30,
        0xD9, 0x4F, 0x94, 0xE7, 0x7C, 0xBB, 0x73, 0xA3, 0x5F, 0xC6, 0xF1, 0xDB,
        0x84, 0x3D, 0x29, 0x72, 0xD1, 0xAD, 0x2D, 0x77, 0x3F, 0x36, 0x24, 0x0F,
        0xC4, 0x12, 0xD7, 0x3C, 0x65, 0x6C, 0xE1, 0x5A, 0x32, 0xAA, 0x0B, 0xA3,
        0xA2, 0x72, 0x33, 0x00, 0x3C, 0x7E, 0x28, 0x36, 0x10, 0x90, 0x38, 0xFB};
    EXPECT_EQ(response, compFirst512B);
}
