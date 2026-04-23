// SPDX-License-Identifier: Apache-2.0

// E2E test for the PLDM firmware-update phase-tracking gap that allowed
// a malicious firmware device to fake a complete firmware update by
// sending TransferComplete/VerifyComplete/ApplyComplete with zero bytes
// ever pulled via RequestFirmwareData.
//
// DSP0267 v1.3.0 Sec. 12.7-9 normatively require the UA to reject out-of-
// sequence completes with PLDM_FWUP_COMMAND_NOT_EXPECTED. Prior to the
// phase-tracking fix, DeviceUpdater had no phase variable and the three
// handlers accepted the FD's self-reported success with no cross-check.
//
// These tests drive the real DeviceUpdater class through the attack
// sequence and verify that every out-of-sequence message is rejected
// with PLDM_FWUP_COMMAND_NOT_EXPECTED, and that the bytes-served check
// in transferComplete catches an FD that claims completion in the
// correct phase but without having pulled any firmware data.
//
// Expected behavior:
//   Against origin/master (unfixed): tests #1-5 FAIL -- handlers return
//   PLDM_SUCCESS and the attack succeeds.
//   Against phase-tracking-fix: tests #1-6 PASS -- handlers return
//   PLDM_FWUP_COMMAND_NOT_EXPECTED on out-of-sequence / insufficient-
//   bytes, and the regression #6 normal-flow test still succeeds.

#include "common/instance_id.hpp"
#include "common/utils.hpp"
#include "fw-update/device_updater.hpp"

#include <libpldm/firmware_update.h>

#include <array>
#include <fstream>
#include <vector>

#include <gtest/gtest.h>

using namespace pldm;
using namespace pldm::fw_update;

namespace
{

// Minimal PLDM request helper. Builds a pldm_msg with the given
// type/command/instance and an optional payload byte.
std::vector<uint8_t> makeRequest(uint8_t command, uint8_t instanceId,
                                 uint8_t payloadByte)
{
    std::vector<uint8_t> msg(sizeof(pldm_msg_hdr) + 1, 0);
    auto* hdr = reinterpret_cast<pldm_msg*>(msg.data());
    hdr->hdr.request = PLDM_REQUEST;
    hdr->hdr.type = PLDM_FWUP;
    hdr->hdr.command = command;
    hdr->hdr.instance_id = instanceId;
    msg[sizeof(pldm_msg_hdr)] = payloadByte;
    return msg;
}

// Build an ApplyComplete request: TransferResult/VerifyResult use one
// byte; ApplyComplete has ApplyResult (1 byte) + ComponentActivation (2).
std::vector<uint8_t> makeApplyComplete(uint8_t instanceId, uint8_t applyResult)
{
    std::vector<uint8_t> msg(sizeof(pldm_msg_hdr) + 3, 0);
    auto* hdr = reinterpret_cast<pldm_msg*>(msg.data());
    hdr->hdr.request = PLDM_REQUEST;
    hdr->hdr.type = PLDM_FWUP;
    hdr->hdr.command = PLDM_APPLY_COMPLETE;
    hdr->hdr.instance_id = instanceId;
    msg[sizeof(pldm_msg_hdr)] = applyResult;
    // ComponentActivationMethodsModification bitfield16 = 0, 0
    return msg;
}

// Fabricate a successful UpdateComponent RESPONSE message. Used to
// advance DeviceUpdater's phase from Inactive to Download without
// sending a real MCTP message round-trip.
std::vector<uint8_t> makeUpdateComponentOkResponse(uint8_t instanceId)
{
    std::vector<uint8_t> msg(
        sizeof(pldm_msg_hdr) + sizeof(pldm_update_component_resp), 0);
    auto* hdr = reinterpret_cast<pldm_msg*>(msg.data());
    hdr->hdr.request = PLDM_RESPONSE;
    hdr->hdr.type = PLDM_FWUP;
    hdr->hdr.command = PLDM_UPDATE_COMPONENT;
    hdr->hdr.instance_id = instanceId;
    // completion_code = 0, comp_compatibility_resp = 0 (COMPONENT_CAN_BE_
    // UPDATED), comp_compatibility_resp_code = 0, update_option_flags_
    // enabled = 0, time_before_req_fw_data = 0.
    return msg;
}

// Extract the completion code byte from a PLDM response produced by
// DeviceUpdater.
uint8_t completionCodeOf(const Response& resp)
{
    EXPECT_GE(resp.size(), sizeof(pldm_msg_hdr) + 1);
    return resp[sizeof(pldm_msg_hdr)];
}

} // namespace

class PhaseTrackingTest : public testing::Test
{
  protected:
    PhaseTrackingTest() :
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
        // compSize (field 6) = 1024; we use this to check the bytes-
        // served gate below.
        compImageInfos = {
            {10, 100, 0xFFFFFFFF, 0, 0, 139, 1024, "VersionString3"}};
        compInfo = {{std::make_pair(10, 100), 1}};
    }

    std::ifstream package;
    FirmwareDeviceIDRecord fwDeviceIDRecord;
    ComponentImageInfos compImageInfos;
    ComponentInfo compInfo;

    // Construct a fresh DeviceUpdater in phase Inactive (initial state).
    // updateManager is nullptr; the fix's new code paths guard this.
    DeviceUpdater makeUpdater()
    {
        return DeviceUpdater(0, package, fwDeviceIDRecord, compImageInfos,
                             compInfo, 512, nullptr);
    }

    // Advance the DeviceUpdater's internal phase from Inactive to
    // Download by handing it a fabricated UpdateComponent success
    // response. This does not exercise updateManager (which is null).
    void advanceToDownload(DeviceUpdater& updater)
    {
        auto resp = makeUpdateComponentOkResponse(0x42);
        auto* respMsg = reinterpret_cast<const pldm_msg*>(resp.data());
        updater.updateComponent(0, respMsg, resp.size() - sizeof(pldm_msg_hdr));
    }
};

// ============================================================
// Phase-gate rejection tests: each FD-initiated message sent in
// the wrong phase must receive PLDM_FWUP_COMMAND_NOT_EXPECTED.
// ============================================================

TEST_F(PhaseTrackingTest, TransferCompleteRejectedBeforeDownload)
{
    // Attack variant: FD sends TransferComplete without UpdateComponent
    // ever completing. Phase stays Inactive; Sec. 12.7 defense fires.
    auto updater = makeUpdater();

    auto req =
        makeRequest(PLDM_TRANSFER_COMPLETE, 0x01, PLDM_FWUP_TRANSFER_SUCCESS);
    auto* reqMsg = reinterpret_cast<const pldm_msg*>(req.data());
    auto response = updater.transferComplete(reqMsg, 1);

    EXPECT_EQ(completionCodeOf(response), PLDM_FWUP_COMMAND_NOT_EXPECTED)
        << "TransferComplete arriving in Inactive phase must be "
           "rejected per DSP0267 Sec. 12.7";
}

TEST_F(PhaseTrackingTest, VerifyCompleteRejectedBeforeTransferComplete)
{
    // Attack variant: FD sends VerifyComplete while UA is in Download
    // phase (transfer has not concluded successfully). Sec. 12.8 defense.
    auto updater = makeUpdater();
    advanceToDownload(updater);

    auto req =
        makeRequest(PLDM_VERIFY_COMPLETE, 0x02, PLDM_FWUP_VERIFY_SUCCESS);
    auto* reqMsg = reinterpret_cast<const pldm_msg*>(req.data());
    auto response = updater.verifyComplete(reqMsg, 1);

    EXPECT_EQ(completionCodeOf(response), PLDM_FWUP_COMMAND_NOT_EXPECTED)
        << "VerifyComplete arriving in Download phase must be "
           "rejected per DSP0267 Sec. 12.8";
}

TEST_F(PhaseTrackingTest, ApplyCompleteRejectedBeforeVerifyComplete)
{
    // Attack variant: FD sends ApplyComplete while UA is in Download
    // phase. Sec. 12.9 defense.
    auto updater = makeUpdater();
    advanceToDownload(updater);

    auto req = makeApplyComplete(0x03, PLDM_FWUP_APPLY_SUCCESS);
    auto* reqMsg = reinterpret_cast<const pldm_msg*>(req.data());
    auto response = updater.applyComplete(reqMsg, 3);

    EXPECT_EQ(completionCodeOf(response), PLDM_FWUP_COMMAND_NOT_EXPECTED)
        << "ApplyComplete arriving in Download phase must be "
           "rejected per DSP0267 Sec. 12.9";
}

TEST_F(PhaseTrackingTest, RequestFirmwareDataRejectedBeforeUpdateComponent)
{
    // Attack variant: FD sends RequestFirmwareData while UA is in
    // Inactive phase (no UpdateComponent succeeded yet). Sec. 12.6.
    auto updater = makeUpdater();

    constexpr std::array<uint8_t, sizeof(pldm_msg_hdr) +
                                      sizeof(pldm_request_firmware_data_req)>
        req{0x8A, 0x05, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00};
    auto* reqMsg = reinterpret_cast<const pldm_msg*>(req.data());
    auto response =
        updater.requestFwData(reqMsg, sizeof(pldm_request_firmware_data_req));

    EXPECT_EQ(completionCodeOf(response), PLDM_FWUP_COMMAND_NOT_EXPECTED)
        << "RequestFirmwareData arriving in Inactive phase must be "
           "rejected per DSP0267 Sec. 12.6";
}

// ============================================================
// Bytes-served gate: FD is in Download phase (UpdateComponent
// succeeded) but claims TransferComplete(success) without having
// pulled any / enough firmware data.
// ============================================================

TEST_F(PhaseTrackingTest, TransferCompleteRejectedWhenZeroBytesServed)
{
    // This is the exact attack documented in the openbmc-security
    // disclosure email: FD drives the handshake through
    // UpdateComponent -> TransferComplete(success) without issuing a
    // single RequestFirmwareData. The phase gate is in Download, so
    // phase alone permits the message; the bytes-served cross-check
    // in transferComplete is what catches it.
    auto updater = makeUpdater();
    advanceToDownload(updater);

    // No requestFwData calls -- currentComponentBytesServed stays at 0,
    // compSize is 1024.
    auto req =
        makeRequest(PLDM_TRANSFER_COMPLETE, 0x04, PLDM_FWUP_TRANSFER_SUCCESS);
    auto* reqMsg = reinterpret_cast<const pldm_msg*>(req.data());
    auto response = updater.transferComplete(reqMsg, 1);

    EXPECT_EQ(completionCodeOf(response), PLDM_FWUP_COMMAND_NOT_EXPECTED)
        << "TransferComplete(success) with zero bytes served must be "
           "rejected (bytes-served defense-in-depth check).";
}

// ============================================================
// Full attack chain: reproduce the exact sequence described in
// the openbmc-security disclosure email.
// ============================================================

TEST_F(PhaseTrackingTest, FullAttackChainBlocked)
{
    auto updater = makeUpdater();

    // Step 1: UA sends RequestUpdate (we skip this in the test; it
    // would succeed against a compliant FD). FD responds to
    // UpdateComponent with success, placing UA in Download.
    advanceToDownload(updater);

    // Step 2: FD sends TransferComplete(success) without any
    // RequestFirmwareData. Bytes-served gate must fire.
    {
        auto req = makeRequest(PLDM_TRANSFER_COMPLETE, 0x10,
                               PLDM_FWUP_TRANSFER_SUCCESS);
        auto* reqMsg = reinterpret_cast<const pldm_msg*>(req.data());
        auto response = updater.transferComplete(reqMsg, 1);
        EXPECT_EQ(completionCodeOf(response), PLDM_FWUP_COMMAND_NOT_EXPECTED);
    }

    // Step 3: After step 2's rejection, phase is Inactive. FD
    // nevertheless sends VerifyComplete(success). Phase gate must
    // fire.
    {
        auto req =
            makeRequest(PLDM_VERIFY_COMPLETE, 0x11, PLDM_FWUP_VERIFY_SUCCESS);
        auto* reqMsg = reinterpret_cast<const pldm_msg*>(req.data());
        auto response = updater.verifyComplete(reqMsg, 1);
        EXPECT_EQ(completionCodeOf(response), PLDM_FWUP_COMMAND_NOT_EXPECTED);
    }

    // Step 4: FD sends ApplyComplete(success). Phase gate must fire.
    {
        auto req = makeApplyComplete(0x12, PLDM_FWUP_APPLY_SUCCESS);
        auto* reqMsg = reinterpret_cast<const pldm_msg*>(req.data());
        auto response = updater.applyComplete(reqMsg, 3);
        EXPECT_EQ(completionCodeOf(response), PLDM_FWUP_COMMAND_NOT_EXPECTED);
    }

    // If all three rejections fired the UA would never have advanced
    // to the "activate firmware" step, so nothing is reported as
    // successfully updated -- which is the point.
}

// ============================================================
// Regression: the happy path still works. Serve all 1024 bytes
// of the component via requestFwData, then TransferComplete must
// succeed (bytes-served >= compSize), and phase advances to Verify.
// ============================================================

TEST_F(PhaseTrackingTest, NormalFlow_TransferCompleteAcceptedAfterFullServe)
{
    auto updater = makeUpdater();
    advanceToDownload(updater);

    // Serve 512 bytes at offset 0, then 512 bytes at offset 512. This
    // matches how a compliant FD would pull the 1024-byte component
    // image with a 512-byte max transfer size.
    auto issueFwDataRequest = [&](uint32_t offset, uint32_t length,
                                  uint8_t instanceId) {
        std::vector<uint8_t> req(
            sizeof(pldm_msg_hdr) + sizeof(pldm_request_firmware_data_req), 0);
        auto* hdr = reinterpret_cast<pldm_msg*>(req.data());
        hdr->hdr.request = PLDM_REQUEST;
        hdr->hdr.type = PLDM_FWUP;
        hdr->hdr.command = PLDM_REQUEST_FIRMWARE_DATA;
        hdr->hdr.instance_id = instanceId;
        auto* payload = reinterpret_cast<pldm_request_firmware_data_req*>(
            req.data() + sizeof(pldm_msg_hdr));
        payload->offset = offset;
        payload->length = length;
        auto* reqMsg = reinterpret_cast<const pldm_msg*>(req.data());
        return updater.requestFwData(reqMsg,
                                     sizeof(pldm_request_firmware_data_req));
    };
    auto r1 = issueFwDataRequest(0, 512, 0x20);
    EXPECT_EQ(completionCodeOf(r1), PLDM_SUCCESS);
    auto r2 = issueFwDataRequest(512, 512, 0x21);
    EXPECT_EQ(completionCodeOf(r2), PLDM_SUCCESS);

    // TransferComplete(success) with bytes-served == compSize must
    // now be accepted and phase advance to Verify.
    auto tcReq =
        makeRequest(PLDM_TRANSFER_COMPLETE, 0x22, PLDM_FWUP_TRANSFER_SUCCESS);
    auto* tcMsg = reinterpret_cast<const pldm_msg*>(tcReq.data());
    auto tcResp = updater.transferComplete(tcMsg, 1);
    EXPECT_EQ(completionCodeOf(tcResp), PLDM_SUCCESS)
        << "Normal-flow TransferComplete (bytes served fully) must be "
           "accepted after the phase-tracking fix";

    // With phase now at Verify, VerifyComplete(success) must be
    // accepted too.
    auto vcReq =
        makeRequest(PLDM_VERIFY_COMPLETE, 0x23, PLDM_FWUP_VERIFY_SUCCESS);
    auto* vcMsg = reinterpret_cast<const pldm_msg*>(vcReq.data());
    auto vcResp = updater.verifyComplete(vcMsg, 1);
    EXPECT_EQ(completionCodeOf(vcResp), PLDM_SUCCESS);
}
